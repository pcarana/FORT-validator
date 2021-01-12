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
#include "monitor/impl/prom_procfs_i.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* Private */
#include "monitor/impl/prom_log.h"

static int
prom_procfs_ensure_buf_size(prom_procfs_buf_t *self)
{
	char *tmp;

	if (self == NULL)
		return -EINVAL;

	if (self->allocated >= self->size + 1)
		return 0;

	while (self->allocated < self->size + 1)
		self->allocated <<= 1;

	tmp = realloc(self->buf, self->allocated);
	if (tmp == NULL)
		return pr_enomem();

	self->buf = tmp;

	return 0;
}

prom_procfs_buf_t *
prom_procfs_buf_new(const char *path)
{
	FILE *f;
	prom_procfs_buf_t *self;
	char errbuf[100];
	unsigned short int initial_size;
	int current_char;
	int i;
	int error;

	f = fopen(path, "r");
	if (f == NULL) {
		PROM_LOG(errbuf);
		pr_op_errno(errno, "Opening '%s'", path);
		return NULL;
	}

#define PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f)	\
error = fclose(f);				\
if (error) {					\
	PROM_LOG(errbuf);			\
	pr_op_errno(errno, "Calling fclose()");	\
}

	initial_size = 32;
	self = malloc(sizeof(prom_procfs_buf_t));
	if (self == NULL) {
		PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f);
		pr_enomem();
		return NULL;
	}

	self->buf = malloc(initial_size);
	if (self->buf == NULL) {
		PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f);
		free(self);
		pr_enomem();
		return NULL;
	}

	self->size = 0;
	self->index = 0;
	self->allocated = initial_size;

	for (current_char = getc(f), i = 0; current_char != EOF;
	    current_char = getc(f), i++) {
		error = prom_procfs_ensure_buf_size(self);
		if (error) {
			prom_procfs_buf_destroy(self);
			PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f);
			return NULL;
		}
		self->buf[i] = current_char;
		self->size++;
	}

	error = prom_procfs_ensure_buf_size(self);
	if (error) {
		prom_procfs_buf_destroy(self);
		PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f);
		return NULL;
	}

	self->buf[self->size] = '\0';
	self->size++;

	PROM_PROCFS_BUF_NEW_HANDLE_F_CLOSE(f);
	if (error)
		return NULL;

	return self;
}

int
prom_procfs_buf_destroy(prom_procfs_buf_t *self)
{
	if (self == NULL)
		return 0;

	free(self->buf);
	free(self);
	self = NULL;

	return 0;
}
