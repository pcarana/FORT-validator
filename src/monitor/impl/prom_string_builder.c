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
#include "monitor/impl/prom_string_builder_i.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

/* Private */
#include "monitor/impl/prom_string_builder_t.h"

/* The initial size of a string created via prom_string_builder */
#define PROM_STRING_BUILDER_INIT_SIZE 32

/* Prototype declaration */
int prom_string_builder_init(prom_string_builder_t *);

struct prom_string_builder {
	/* the target string */
	char *str;
	/* the size allocated to the string in bytes */
	size_t allocated;
	/* the length of str */
	size_t len;
	/* the initialize size of space to allocate */
	size_t init_size;
};

prom_string_builder_t *
prom_string_builder_new(void)
{
	prom_string_builder_t *self;
	int error;

	self = malloc(sizeof(prom_string_builder_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->init_size = PROM_STRING_BUILDER_INIT_SIZE;
	error = prom_string_builder_init(self);
	if (error) {
		prom_string_builder_destroy(self);
		return NULL;
	}

	return self;
}

int
prom_string_builder_init(prom_string_builder_t *self)
{
	if (self == NULL)
		return -EINVAL;

	self->str = malloc(self->init_size);
	if (self->str == NULL)
		return pr_enomem();

	*self->str = '\0';
	self->allocated = self->init_size;
	self->len = 0;

	return 0;
}

int
prom_string_builder_destroy(prom_string_builder_t *self)
{
	if (self == NULL)
		return 0;

	free(self->str);
	self->str = NULL;
	free(self);
	self = NULL;

	return 0;
}

/*
 * @brief API PRIVATE Grows the size of the string given the value we want to
 * add
 *
 * The method continuously shifts left until the new size is large enough to
 * accommodate add_len. This private method is called in methods that need to
 * add one or more characters to the underlying string.
 */
static int
prom_string_builder_ensure_space(prom_string_builder_t *self, size_t add_len)
{
	char *tmp;

	if (self == NULL)
		return -EINVAL;

	if (add_len == 0 || self->allocated >= self->len + add_len + 1)
		return 0;

	while (self->allocated < self->len + add_len + 1)
		self->allocated <<= 1;

	tmp = realloc(self->str, self->allocated);
	if (tmp == NULL)
		return pr_enomem();

	self->str = tmp;
	return 0;
}

int
prom_string_builder_add_str(prom_string_builder_t *self, const char *str)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	if (str == NULL || *str == '\0')
		return 0;

	size_t len = strlen(str);
	error = prom_string_builder_ensure_space(self, len);
	if (error)
		return error;

	memcpy(self->str + self->len, str, len);
	self->len += len;
	self->str[self->len] = '\0';

	return 0;
}

int
prom_string_builder_add_char(prom_string_builder_t *self, char c)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_string_builder_ensure_space(self, 1);
	if (error)
		return error;

	self->str[self->len] = c;
	self->len++;
	self->str[self->len] = '\0';

	return 0;
}

int
prom_string_builder_truncate(prom_string_builder_t *self, size_t len)
{
	if (self == NULL)
		return -EINVAL;

	if (len >= self->len)
		return 0;

	self->len = len;
	self->str[self->len] = '\0';

	return 0;
}

int
prom_string_builder_clear(prom_string_builder_t *self)
{
	if (self == NULL)
		return 0;

	free(self->str);
	self->str = NULL;

	return prom_string_builder_init(self);
}

size_t
prom_string_builder_len(prom_string_builder_t *self)
{
	return self->len;
}

char *
prom_string_builder_dump(prom_string_builder_t *self)
{
	char *out;

	if (self == NULL)
		return NULL;

	/* +1 to accommodate \0 */
	out = malloc((self->len + 1) * sizeof(char));
	if (out == NULL) {
		pr_enomem();
		return NULL;
	}
	memcpy(out, self->str, self->len + 1);

	return out;
}

char *
prom_string_builder_str(prom_string_builder_t *self)
{
	return self->str;
}
