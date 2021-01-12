/**
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
#include "monitor/impl/prom_process_limits_i.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "log.h"

/* Public */
#include "monitor/def/prom_gauge.h"

/* Private */
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_process_limits_t.h"
#include "monitor/impl/prom_procfs_i.h"

const char PROM_PROCESS_LIMITS_RDP_LETTERS[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

const char PROM_PROCESS_LIMITS_RDP_DIGITS[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

const char *PROM_PROCESS_LIMITS_RDP_UNLIMITED = "unlimited";

typedef enum prom_process_limit_rdp_limit_type
{
	PROM_PROCESS_LIMITS_RDP_SOFT,
	PROM_PROCESS_LIMITS_RDP_HARD,
} prom_process_limit_rdp_limit_type_t;

prom_gauge_t *prom_process_virtual_memory_max_bytes;
prom_gauge_t *prom_process_resident_memory_bytes;
prom_gauge_t *prom_process_max_fds;

/*
 * prom_process_limits_row_t
 */

prom_process_limits_row_t *
prom_process_limits_row_new(const char *limit, const int soft, const int hard,
    const char *units)
{
	prom_process_limits_row_t *self;

	self = malloc(sizeof(prom_process_limits_row_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->limit = strdup(limit);
	if (self->limit == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	self->units = strdup(units);
	if (self->units == NULL) {
		free((void *)self->limit);
		free(self);
		pr_enomem();
		return NULL;
	}

	self->soft = soft;
	self->hard = hard;

	return self;
}

int
prom_process_limits_row_destroy(prom_process_limits_row_t *self)
{
	if (self == NULL)
		return 0;

	free((void *)self->limit);
	self->limit = NULL;

	free((void *)self->units);
	self->units = NULL;

	free(self);
	self = NULL;

	return 0;
}

prom_process_limits_current_row_t *
prom_process_limits_current_row_new(void)
{
	prom_process_limits_current_row_t *self;

	self = malloc(sizeof(prom_process_limits_current_row_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->limit = NULL;
	self->soft = 0;
	self->hard = 0;
	self->units = NULL;

	return self;
}

int
prom_process_limits_current_row_set_limit(
    prom_process_limits_current_row_t *self, char *limit)
{
	if (self == NULL)
		return -EINVAL;
	self->limit = strdup(limit);
	if (self->limit == NULL)
		return pr_enomem();

	return 0;
}

int
prom_process_limits_current_row_set_units(
    prom_process_limits_current_row_t *self, char *units)
{
	if (self == NULL)
		return -EINVAL;
	self->units = strdup(units);
	if (self->units == NULL)
		return pr_enomem();

	return 0;
}

int
prom_process_limits_current_row_clear(prom_process_limits_current_row_t *self)
{
	if (self == NULL)
		return -EINVAL;
	if (self->limit) {
		free((void *)self->limit);
		self->limit = NULL;
	}
	if (self->units) {
		free((void *)self->units);
		self->units = NULL;
	}
	self->soft = 0;
	self->hard = 0;

	return 0;
}

int
prom_process_limits_current_row_destroy(prom_process_limits_current_row_t *self)
{
	int error;

	if (self == NULL)
		return 0;

	error = prom_process_limits_current_row_clear(self);
	free(self);
	self = NULL;

	return error;
}

/*
 * prom_process_limits_file_t
 */

prom_process_limits_file_t *
prom_process_limits_file_new(const char *path)
{
	char buf[255];
	int pid;

	if (path)
		return prom_procfs_buf_new(path);

	pid = (int) getpid();
	sprintf(buf, "/proc/%d/limits", pid);

	return prom_procfs_buf_new(buf);
}

int
prom_process_limits_file_destroy(prom_process_limits_file_t *self)
{
	int error;

	if (self == NULL)
		return 0;

	error = prom_procfs_buf_destroy(self);
	self = NULL;
	return error;
}

static void
prom_process_limits_file_free_map_item_fn(void *gen)
{
	prom_process_limits_row_t *row;

	row = (prom_process_limits_row_t *)gen;
	prom_process_limits_row_destroy(row);
	row = NULL;
}

/*
 * @brief Returns a map. Each key is a key in /proc/[pid]/limits. Each value is
 * a pointer to a prom_process_limits_row_t. Returns NULL upon failure.
 *
 * EBNF
 *
 * limits_file = first_line , data_line , { data_line } ;
 * first_line = character, { character } , "\n" ;
 * character = " " | letter | digit ;
 * letter = "A" | "B" | "C" | "D" | "E" | "F" | "G"
 *        | "H" | "I" | "J" | "K" | "L" | "M" | "N"
 *        | "O" | "P" | "Q" | "R" | "S" | "T" | "U"
 *        | "V" | "W" | "X" | "Y" | "Z" | "a" | "b"
 *        | "c" | "d" | "e" | "f" | "g" | "h" | "i"
 *        | "j" | "k" | "l" | "m" | "n" | "o" | "p"
 *        | "q" | "r" | "s" | "t" | "u" | "v" | "w"
 *        | "x" | "y" | "z" ;
 * digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
 * data_line = limit , space, space, { " " }, soft_limit, " ", " ", { " " },
 * hard_limit, " ", " ", { " " }, { units }, { space_char }, "\n" ;
 * space_char = " " | "\t" ;
 * limit = { word_and_space } , word ;
 * word_and_space = word, " " ;
 * word = letter, { letter } ;
 * soft_limit = ( digit, { digit } ) | "unlimited" ;
 * hard_limit = soft_limit ;
 * units = word ;
 */
prom_map_t *
prom_process_limits(prom_process_limits_file_t *f)
{
	prom_process_limits_current_row_t *current_row;
	prom_map_t *m;
	int error;

	m = prom_map_new();
	if (m == NULL)
		return NULL;

	error = prom_map_set_free_value_fn(m,
	    &prom_process_limits_file_free_map_item_fn);
	if (error) {
		prom_map_destroy(m);
		return NULL;
	}

	current_row = prom_process_limits_current_row_new();
	if (current_row == NULL) {
		prom_map_destroy(m);
		return NULL;
	}

	if (!prom_process_limits_rdp_file(f, m, current_row)) {
		prom_process_limits_current_row_destroy(current_row);
		prom_map_destroy(m);
		return NULL;
	}

	error = prom_process_limits_current_row_destroy(current_row);
	current_row = NULL;
	if (error) {
		prom_map_destroy(m);
		return NULL;
	}

	return m;
}

bool
prom_process_limits_rdp_file(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	if (!prom_process_limits_rdp_first_line(f, map, current_row))
		return false;

	while (f->index < f->size - 1) {
		if (!prom_process_limits_rdp_data_line(f, map, current_row))
			return false;
	}

	return true;
}

bool
prom_process_limits_rdp_first_line(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	while (prom_process_limits_rdp_character(f, map, current_row)) { }

	if (f->buf[f->index] == '\n') {
		f->index++;
		return true;
	}

	return false;
}

bool
prom_process_limits_rdp_character(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	if (prom_process_limits_rdp_letter(f, map, current_row))
		return true;
	if (prom_process_limits_rdp_digit(f, map, current_row))
		return true;
	if (f->buf[f->index] == ' ' && f->buf[f->index] < f->size - 1) {
		f->index++;
		return true;
	}

	return false;
}

bool
prom_process_limits_rdp_letter(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	unsigned int size;
	int letter;
	int in_buff;
	int i;

	if (f->index >= f->size - 1)
		return false;

	size = sizeof(PROM_PROCESS_LIMITS_RDP_LETTERS);
	for (i = 0; i < size; i++) {
		letter = PROM_PROCESS_LIMITS_RDP_LETTERS[i];
		in_buff = f->buf[f->index];
		if (letter == in_buff) {
			f->index++;
			return true;
		}
	}

	return false;
}

bool
prom_process_limits_rdp_digit(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	unsigned int size;
	int digit;
	int in_buff;
	int i;

	if (f->index >= f->size - 1)
		return false;

	size = sizeof(PROM_PROCESS_LIMITS_RDP_DIGITS);
	for (i = 0; i < size; i++) {
		digit = PROM_PROCESS_LIMITS_RDP_DIGITS[i];
		in_buff = f->buf[f->index];
		if (digit == in_buff) {
			f->index++;
			return true;
		}
	}

	return false;
}

bool
prom_process_limits_rdp_data_line(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	prom_process_limits_row_t *row;
	const char *limit;
	const char *units;
	int soft;
	int hard;

	/*
	 * Process and parse data line, loading relevant data into the
	 * current_row
	 */
	if (!prom_process_limits_rdp_limit(f, map, current_row))
		return false;

	prom_process_limits_rdp_next_token(f);
	if (!prom_process_limits_rdp_soft_limit(f, map, current_row))
		return false;

	prom_process_limits_rdp_next_token(f);
	if (!prom_process_limits_rdp_hard_limit(f, map, current_row))
		return false;

	prom_process_limits_rdp_next_token(f);
	prom_process_limits_rdp_units(f, map, current_row);

	/* Load data from the current row into the map */
	limit = (const char*) current_row->limit;
	soft = current_row->soft;
	hard = current_row->hard;
	units = (const char*) current_row->units;
	row = prom_process_limits_row_new(limit, soft, hard, units);
	if (row == NULL)
		return false;

	prom_map_set(map, limit, row);
	prom_process_limits_current_row_clear(current_row);

	/* Progress to the next token */
	prom_process_limits_rdp_next_token(f);
	return true;
}

/*
 * @brief EBNF: space_char = " " | "\t" ;
 */
bool
prom_process_limits_rdp_space_char(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	char c;

	c = f->buf[f->index];
	if (c == ' ' || c == '\t') {
		f->index++;
		return true;
	}
	return false;
}

/*
 * @brief EBNF: limit = { word_and_space } , word ;
 */
bool
prom_process_limits_rdp_limit(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	size_t current_index;
	size_t size;
	char *limit;
	int i;

	current_index = f->index;
	while (prom_process_limits_rdp_word_and_space(f, map, current_row)) { }

	if (prom_process_limits_rdp_word(f, map, current_row)) {
		/* Add one for \0 */
		size = f->index - current_index + 1;
		limit = malloc(size);
		if (limit == NULL) {
			pr_enomem();
			return false;
		}
		for (i = 0; i < size - 1; i++) {
			limit[i] = f->buf[current_index + i];
		}
		limit[size - 1] = '\0';
		prom_process_limits_current_row_set_limit(current_row, limit);
		free(limit);
		return true;
	}

	return false;
}

/*
 * @brief EBNF: word_and_space = letter, { letter }, " " ;
 */
bool
prom_process_limits_rdp_word_and_space(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	size_t current_index;

	current_index = f->index;
	if (prom_process_limits_rdp_word(f, map, current_row)
	    && f->buf[f->index] == ' ') {
		f->index++;
		if (f->index + 1 < f->size && f->buf[f->index + 1] != ' '
		    && f->buf[f->index + 1] != '\t') {
			return true;
		}
	}
	f->index = current_index;
	return false;
}

bool
prom_process_limits_rdp_word(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	size_t original_index;

	original_index = f->index;
	while (prom_process_limits_rdp_letter(f, map, current_row)) { }

	return (f->index - original_index) > 0;
}

static bool
prom_process_limits_rdp_generic_limit(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row,
    prom_process_limit_rdp_limit_type_t type)
{
	size_t current_index;
	size_t num_digits;
	size_t i;
	char *buf;
	int value;

	current_index = f->index;
	value = 0;

	if (prom_process_limits_rdp_match(f,
	    PROM_PROCESS_LIMITS_RDP_UNLIMITED)) {
		value = -1;
	} else {
		while (prom_process_limits_rdp_digit(f, map, current_row)) { }
		num_digits = f->index - current_index + 1;
		if (num_digits <= 0)
			return false;

		buf = malloc(num_digits + 1);
		if (buf == NULL) {
			pr_enomem();
			return false;
		}

		for (i = 0; i < num_digits - 1; i++)
			buf[i] = f->buf[current_index + i];

		buf[num_digits - 1] = '\0';
		value = atoi(buf);
		f->index += num_digits;
		free(buf);
	}

	switch (type) {
	case PROM_PROCESS_LIMITS_RDP_SOFT:
		current_row->soft = value;
		break;
	case PROM_PROCESS_LIMITS_RDP_HARD:
		current_row->hard = value;
		break;
	}

	return true;
}

bool
prom_process_limits_rdp_soft_limit(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	return prom_process_limits_rdp_generic_limit(f, map, current_row,
	    PROM_PROCESS_LIMITS_RDP_SOFT);
}

bool
prom_process_limits_rdp_hard_limit(prom_process_limits_file_t *f,
    prom_map_t *map, prom_process_limits_current_row_t *current_row)
{
	return prom_process_limits_rdp_generic_limit(f, map, current_row,
	    PROM_PROCESS_LIMITS_RDP_HARD);
}

bool
prom_process_limits_rdp_units(prom_process_limits_file_t *f, prom_map_t *map,
    prom_process_limits_current_row_t *current_row)
{
	size_t current_index;
	size_t num_chars;
	size_t i;
	char *buf;

	current_index = f->index;
	if (prom_process_limits_rdp_word(f, map, current_row)) {
		num_chars = f->index - current_index + 1;

		buf = malloc(num_chars);
		if (buf == NULL) {
			pr_enomem();
			return false;
		}
		for (i = 0; i < num_chars - 1; i++) {
			buf[i] = f->buf[current_index + i];
		}
		buf[num_chars - 1] = '\0';
		prom_process_limits_current_row_set_units(current_row, buf);
		free(buf);
		return true;
	}
	return false;
}

int
prom_process_limits_rdp_next_token(prom_process_limits_file_t *f)
{
	while (f->buf[f->index] == ' ' || f->buf[f->index] == '\n'
	    || f->buf[f->index] == '\t') {
		f->index++;
	}
	return 0;
}

bool
prom_process_limits_rdp_match(prom_process_limits_file_t *f, const char *token)
{
	size_t current_index;
	size_t i;

	current_index = f->index;
	for (i = 0; i < strlen(token); i++) {
		if (f->buf[current_index + i] != token[i])
			return false;
	}
	f->index += strlen(token);
	return true;
}

/*
 * @brief Initializes each gauge metric found in prom_process_t.h
 */
int
prom_process_limits_init(void)
{
	prom_process_max_fds = prom_gauge_new("process_max_fds",
	    "Maximum number of open file descriptors.", 0, NULL);
	if (prom_process_max_fds == NULL)
		return pr_enomem();

	prom_process_virtual_memory_max_bytes = prom_gauge_new(
	    "process_virtual_memory_max_bytes",
	    "Maximum amount of virtual memory available in bytes.", 0,
	    NULL);
	if (prom_process_virtual_memory_max_bytes == NULL) {
		prom_gauge_destroy(prom_process_max_fds);
		return pr_enomem();
	}

	return 0;
}

/*
 * FIXME (now) Add destroy method (maybe 'prom_process_limits_destroy'), it must
 * destroy:
 * - prom_process_max_fds
 * - prom_process_virtual_memory_max_bytes
 */
