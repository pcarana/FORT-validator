/*
 * Copyright 2019-2020 DigitalOcean Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "monitor/impl/prom_process_fds_i.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

/* Public */
#include "monitor/def/prom_gauge.h"

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_process_fds_t.h"

prom_gauge_t *prom_process_open_fds;

int
prom_process_fds_count(const char *path)
{
	struct dirent *de;
	DIR *d;
	char p[50];
	int pid;
	int count;
	int error;

	if (path) {
		d = opendir(path);
		if (d == NULL) {
			PROM_LOG(PROM_STDIO_OPEN_DIR_ERROR);
			return pr_op_errno(errno, PROM_STDIO_OPEN_DIR_ERROR);
		}
	} else {
		pid = (int)getpid();
		sprintf(p, "/proc/%d/fd", pid);
		d = opendir(p);
		if (d == NULL) {
			PROM_LOG(PROM_STDIO_OPEN_DIR_ERROR);
			return pr_op_errno(errno, PROM_STDIO_OPEN_DIR_ERROR);
		}
	}

	count = 0;
	while ((de = readdir(d)) != NULL) {
		if (strcmp(".", de->d_name) == 0
		    || strcmp("..", de->d_name) == 0)
			continue;
		count++;
	}

	error = closedir(d);
	if (error) {
		PROM_LOG(PROM_STDIO_CLOSE_DIR_ERROR);
		return pr_op_errno(errno, PROM_STDIO_CLOSE_DIR_ERROR);
	}

	return count;
}

int
prom_process_fds_init(void)
{
	prom_process_open_fds = prom_gauge_new("process_open_fds",
	    "Number of open file descriptors.", 0, NULL);
	if (prom_process_open_fds == NULL)
		return -EINVAL;

	return 0;
}

/*
 * FIXME (now) Add destroy method (maybe 'prom_process_fds_destroy'), it must
 * destroy:
 * - prom_process_open_fds
 */
