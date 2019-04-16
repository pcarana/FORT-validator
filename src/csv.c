#include "csv.h"

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "array_list.h"
#include "configuration.h"
#include "address.h"
#include "line_file.h"
#include "vrps.h"

ARRAY_LIST(vrplist, struct vrp)

static int
parse_asn(char *text, unsigned int *value)
{
	long asn;
	char *start;

	if (text == NULL) {
		warnx("Null string received, can't decode ASN");
		return -EINVAL;
	}

	/* The text 'AS' may precede the number */
	start = strchr(text, 'S');
	start = start != NULL ? start + 1 : text;

	errno = 0;
	asn = strtol(start, NULL, 10);
	if (errno) {
		warn("Invalid ASN '%s'", text);
		return -EINVAL;
	}
	/* An underflow or overflow will be considered here */
	if (asn < 0 || UINT32_MAX < asn) {
		warnx("ASN (%ld) is out of range [0 - %u].", asn, UINT32_MAX);
		return -EINVAL;
	}
	*value = (unsigned int) asn;

	return 0;
}

static int
parse_prefix4(char *text, struct ipv4_prefix *prefixv4)
{
	if (text == NULL)
		return -EINVAL;
	return prefix4_decode(text, prefixv4);
}

static int
parse_prefix6(char *text, struct ipv6_prefix *prefixv6)
{
	if (text == NULL)
		return -EINVAL;
	return prefix6_decode(text, prefixv6);
}

static int
parse_prefix_length(char *text, unsigned int *value, int max_value)
{
	if (text == NULL)
		return -EINVAL;
	return prefix_length_decode(text, value, max_value);
}

static int
add_vrp(char *line, struct vrplist *vrplist)
{
	struct ipv4_prefix prefixv4;
	struct ipv6_prefix prefixv6;
	struct vrp vrp;
	unsigned int asn, prefix_length, max_prefix_length;
	int error;
	bool isv4;
	char *token, *line_copy;

	if (strcmp(line, "") == 0) {
		warnx("Empty line.");
		return -EINVAL;
	}

	line_copy = malloc(strlen(line) + 1);
	if (line_copy == NULL) {
		error = -errno;
		warn("Out of memory allocating CSV line copy");
		goto error;
	}
	strcpy(line_copy, line);

	error = 0;

	/* First column: ASN in format "AS###" */
	token = strtok(line_copy, ",");
	error = parse_asn(token, &asn);
	if (error)
		goto error;

	/* Second column (first part): Prefix in string format */
	token = strtok(NULL, "/");
	isv4 = strchr(token, ':') == NULL;
	if (isv4)
		error = parse_prefix4(token, &prefixv4);
	else
		error = parse_prefix6(token, &prefixv6);

	if (error)
		goto error;

	/* Second column (second part): Prefix length in numeric format */
	token = strtok(NULL, ",");
	error = parse_prefix_length(token,
	    isv4 ? &prefixv4.len : &prefixv6.len,
	    isv4 ? 32 : 128);
	if (error)
		goto error;

	/* Third column: Prefix max length in numeric format */
	token = strtok(NULL, ",");
	error = parse_prefix_length(token, &max_prefix_length,
	    isv4 ? 32 : 128);
	if (error)
		goto error;

	/* Now validate the prefix */
	if (isv4) {
		prefix_length = prefixv4.len;
		error = prefix4_validate(&prefixv4);
	} else {
		prefix_length = prefixv6.len;
		error = prefix6_validate(&prefixv6);
	}
	if (error)
		goto error;

	if (prefix_length > max_prefix_length) {
		error = -EINVAL;
		warnx("Prefix length is greater than max prefix length at line '%s'",
		    line);
		goto error;
	}

	if (isv4)
		vrp = create_vrp4(asn, prefixv4.addr, prefixv4.len,
		    max_prefix_length);
	else
		vrp = create_vrp6(asn, prefixv6.addr, prefixv6.len,
		    max_prefix_length);

	error =  vrplist_add(vrplist, &vrp);

error:
	free(line_copy);
	return error;
}

static void
clean_localvrps(struct vrp *vrp)
{
	/* Nothing special to clean, this is just a warning silencer */
}

static int
load_vrps(struct line_file *lfile, bool is_update)
{
	struct vrplist localvrps;
	char *line;
	int current_line;
	int error;

	/* Init delta */
	error = vrplist_init(&localvrps);
	if (error) {
		warnx("Couldn't allocate new VRPs");
		return error;
	}
	current_line = 1;
	do {
		error = lfile_read(lfile, &line);
		if (error) {
			warnx("Error reading line %d, stop processing file.",
			    current_line);
			goto end;
		}
		if (line == NULL) {
			error = 0;
			goto persist;
		}

		error = add_vrp(line, &localvrps);
		if (error)
			warnx("Ignoring content at line %d.", current_line);

		current_line++;
		free(line);
	} while (true);
persist:
	error = deltas_db_create_delta(localvrps.array, localvrps.len);
	if (error)
		warnx("VRPs Delta couldn't be persisted");
end:
	if (line != NULL)
		free(line);
	vrplist_cleanup(&localvrps, clean_localvrps);
	return error;
}

static int
load_vrps_file(bool check_update, bool *updated)
{

	struct line_file *lfile;
	struct stat attr;
	time_t last_update;
	char const *location;
	int error;

	location = config_get_vrps_location();

	/* Look for the last update date */
	error = stat(location, &attr);
	if (error) {
		warnx("Couldn't get last modified date of %s, skip update",
			location);
		error = -ENOENT;
		goto end;
	}

	last_update = attr.st_mtim.tv_sec;
	if (check_update &&
	    difftime(last_update, get_vrps_last_modified_date()) <= 0)
		goto end;

	error = lfile_open(location, &lfile);
	if (error)
		goto end; /* Error msg already printed. */

	error = load_vrps(lfile, check_update);
	if (error)
		goto close_file;

	if (updated != NULL)
		*updated = check_update &&
		    difftime(last_update, get_vrps_last_modified_date()) > 0;

	set_vrps_last_modified_date(last_update);

close_file:
	lfile_close(lfile);
end:
	return error;
}

int
csv_parse_vrps_file(void)
{
	return load_vrps_file(false, NULL);
}

int
csv_check_vrps_file(bool *updated)
{
	return load_vrps_file(true, updated);
}
