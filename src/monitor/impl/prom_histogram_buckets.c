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
#include "monitor/def/prom_histogram_buckets.h"

#include <stdarg.h>
#include <stdlib.h>

/* Private */
#include "monitor/impl/prom_log.h"

prom_histogram_buckets_t *prom_histogram_default_buckets;

prom_histogram_buckets_t *
prom_histogram_buckets_new(size_t count, double bucket, ...)
{
	prom_histogram_buckets_t *self;
	double *upper_bounds;
	va_list arg_list;
	int i;

	self = malloc(sizeof(prom_histogram_buckets_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->count = count;
	upper_bounds = malloc(sizeof(double) * count);
	if (upper_bounds == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	upper_bounds[0] = bucket;
	if (count == 1) {
		self->upper_bounds = upper_bounds;
		return self;
	}

	va_start(arg_list, bucket);
	for (i = 1; i < count; i++)
		upper_bounds[i] = va_arg(arg_list, double);
	va_end(arg_list);
	self->upper_bounds = upper_bounds;

	return self;
}

prom_histogram_buckets_t *
prom_histogram_buckets_linear(double start, double width, size_t count)
{
	prom_histogram_buckets_t *self;
	double *upper_bounds;
	size_t i;

	if (count <= 1)
		return NULL;

	self = malloc(sizeof(prom_histogram_buckets_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	upper_bounds = malloc(sizeof(double) * count);
	if (upper_bounds == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	upper_bounds[0] = start;
	for (i = 1; i < count; i++)
		upper_bounds[i] = upper_bounds[i - 1] + width;

	self->upper_bounds = upper_bounds;
	self->count = count;

	return self;
}

prom_histogram_buckets_t *
prom_histogram_buckets_exponential(double start, double factor, size_t count)
{
	prom_histogram_buckets_t *self;
	double *upper_bounds;
	size_t i;

	if (count < 1) {
		PROM_LOG("count must be less than 1");
		return NULL;
	}
	if (start <= 0) {
		PROM_LOG("start must be less than or equal to 0");
		return NULL;
	}
	if (factor <= 1) {
		PROM_LOG("factor must be less than or equal to 1");
		return NULL;
	}

	self = malloc(sizeof(prom_histogram_buckets_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	upper_bounds = malloc(sizeof(double) * count);
	if (upper_bounds == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	upper_bounds[0] = start;
	for (i = 1; i < count; i++)
		upper_bounds[i] = upper_bounds[i - 1] * factor;

	self->upper_bounds = upper_bounds;
	self->count = count;

	return self;
}

int
prom_histogram_buckets_destroy(prom_histogram_buckets_t *self)
{
	if (self == NULL)
		return 0;

	free((double *)self->upper_bounds);
	self->upper_bounds = NULL;
	free(self);
	self = NULL;

	return 0;
}

size_t
prom_histogram_buckets_count(prom_histogram_buckets_t *self)
{
	return self->count;
}
