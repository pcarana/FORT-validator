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
#include "monitor/def/prom_metric.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "monitor/def/prom_histogram_buckets.h"

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_metric_formatter_i.h"
#include "monitor/impl/prom_metric_i.h"
#include "monitor/impl/prom_metric_sample_histogram_i.h"
#include "monitor/impl/prom_metric_sample_i.h"

char *prom_metric_type_map[4] = { "counter", "gauge", "histogram", "summary" };

prom_metric_t *
prom_metric_new(prom_metric_type_t metric_type, const char *name,
    const char *help, size_t label_key_count, const char **label_keys)
{
	prom_metric_t *self;
	const char **k;
	int i;
	int error;

	self = malloc(sizeof(prom_metric_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->type = metric_type;
	self->name = name;
	self->help = help;
	self->buckets = NULL;

	k = malloc(sizeof(const char *) * label_key_count);
	if (k == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	self->label_keys = k;
	for (i = 0; i < label_key_count; i++) {
		if (strcmp(label_keys[i], "le") == 0 ||
		    strcmp(label_keys[i], "quantile") == 0) {
			PROM_LOG(PROM_METRIC_INVALID_LABEL_NAME);
			self->label_key_count = i;
			goto destroy_self;
		}
		k[i] = strdup(label_keys[i]);
		if (k[i] == NULL) {
			self->label_key_count = i;
			pr_enomem();
			goto destroy_self;
		}
	}
	self->label_key_count = label_key_count;

	self->samples = prom_map_new();
	if (self->samples == NULL)
		goto destroy_self;

	if (metric_type == PROM_HISTOGRAM)
		error = prom_map_set_free_value_fn(self->samples,
		    &prom_metric_sample_histogram_free_generic);
	else
		error = prom_map_set_free_value_fn(self->samples,
		    &prom_metric_sample_free_generic);

	if (error)
		goto destroy_self;

	self->formatter = prom_metric_formatter_new();
	if (self->formatter == NULL)
		goto destroy_self;

	self->rwlock = malloc(sizeof(pthread_rwlock_t));
	if (self->rwlock == NULL) {
		pr_enomem();
		goto destroy_self;
	}

	error = pthread_rwlock_init(self->rwlock, NULL);
	if (error) {
		pr_op_errno(error, PROM_PTHREAD_RWLOCK_INIT_ERROR);
		goto destroy_self;
	}

	return self;
destroy_self:
	prom_metric_destroy(self);
	return NULL;
}

int
prom_metric_destroy(prom_metric_t *self)
{
	int i;
	int ret;
	int error;

	if (self == NULL)
		return 0;

	ret = 0;
	if (self->buckets != NULL) {
		error = prom_histogram_buckets_destroy(self->buckets);
		self->buckets = NULL;
		if (error)
			ret = error;
	}

	error = prom_map_destroy(self->samples);
	self->samples = NULL;
	if (error)
		ret = error;

	error = prom_metric_formatter_destroy(self->formatter);
	self->formatter = NULL;
	if (error)
		ret = error;

	error = pthread_rwlock_destroy(self->rwlock);
	if (error) {
		PROM_LOG(PROM_PTHREAD_RWLOCK_DESTROY_ERROR);
		ret = error;
	}

	free(self->rwlock);
	self->rwlock = NULL;

	for (i = 0; i < self->label_key_count; i++) {
		free((void *)self->label_keys[i]);
		self->label_keys[i] = NULL;
	}
	free(self->label_keys);
	self->label_keys = NULL;

	free(self);
	self = NULL;

	return ret;
}

int
prom_metric_destroy_generic(void *item)
{
	int error;
	prom_metric_t *self;

	self = (prom_metric_t *)item;
	error = prom_metric_destroy(self);
	self = NULL;

	return error;
}

void
prom_metric_free_generic(void *item)
{
	prom_metric_destroy((prom_metric_t *)item);
}

prom_metric_sample_t *
prom_metric_sample_from_labels(prom_metric_t *self, const char **label_values)
{
	prom_metric_sample_t *sample;
	const char *l_value;
	int error;

	if (self == NULL)
		return NULL;

	rwlock_write_lock(self->rwlock);

#define HANDLE_UNLOCK			\
do {					\
	rwlock_unlock(self->rwlock);	\
	return NULL;			\
} while(0);

	/* Get l_value */
	error = prom_metric_formatter_load_l_value(self->formatter, self->name,
	    NULL, self->label_key_count, self->label_keys, label_values);
	if (error)
		HANDLE_UNLOCK

	/* This must be freed before returning */
	l_value = prom_metric_formatter_dump(self->formatter);
	if (l_value == NULL)
		HANDLE_UNLOCK

	/* Get sample */
	sample = (prom_metric_sample_t *)prom_map_get(self->samples, l_value);
	if (sample == NULL) {
		sample = prom_metric_sample_new(self->type, l_value, 0.0);
		if (sample == NULL) {
			free((void *)l_value);
			HANDLE_UNLOCK
		}
		error = prom_map_set(self->samples, l_value, sample);
		if (error) {
			free((void *)l_value);
			HANDLE_UNLOCK
		}
	}
	pthread_rwlock_unlock(self->rwlock);
	free((void *)l_value);

	return sample;
#undef HANDLE_UNLOCK
}

prom_metric_sample_histogram_t *
prom_metric_sample_histogram_from_labels(prom_metric_t *self,
    const char **label_values)
{
	prom_metric_sample_histogram_t *sample;
	const char *l_value;
	int error;

	if (self == NULL)
		return NULL;

	rwlock_write_lock(self->rwlock);

#define HANDLE_UNLOCK			\
do {					\
	rwlock_unlock(self->rwlock);	\
	return NULL;			\
} while(0);

	/* Load the l_value */
	error = prom_metric_formatter_load_l_value(self->formatter, self->name,
	    NULL, self->label_key_count, self->label_keys, label_values);
	if (error)
		HANDLE_UNLOCK

	/* This must be freed before returning */
	l_value = prom_metric_formatter_dump(self->formatter);
	if (l_value == NULL)
		HANDLE_UNLOCK

	/* Get sample */
	sample = (prom_metric_sample_histogram_t *)prom_map_get(self->samples,
	    l_value);
	if (sample == NULL) {
		sample = prom_metric_sample_histogram_new(self->name,
		    self->buckets, self->label_key_count, self->label_keys,
		    label_values);
		if (sample == NULL) {
			free((void *)l_value);
			HANDLE_UNLOCK
		}
		error = prom_map_set(self->samples, l_value, sample);
		if (error) {
			free((void *)l_value);
			HANDLE_UNLOCK;
		}
	}
	pthread_rwlock_unlock(self->rwlock);
	free((void *)l_value);

	return sample;
#undef HANDLE_UNLOCK
}
