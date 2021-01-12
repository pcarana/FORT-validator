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
#include "monitor/def/prom_metric_sample_histogram.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* Public */
#include "monitor/def/prom_histogram.h"

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_linked_list_i.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_metric_formatter_i.h"
#include "monitor/impl/prom_metric_sample_histogram_i.h"
#include "monitor/impl/prom_metric_sample_i.h"

/*
 * Static Declarations
 */

static const char *prom_metric_sample_histogram_l_value_for_bucket(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **, double);

static const char *prom_metric_sample_histogram_l_value_for_inf(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **);

static void prom_metric_sample_histogram_free_str_generic(void *);

static int prom_metric_sample_histogram_init_bucket_samples(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **);

static int prom_metric_sample_histogram_init_inf(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **);

static int prom_metric_sample_histogram_init_count(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **);

static int prom_metric_sample_histogram_init_summary(
    prom_metric_sample_histogram_t *, const char *, size_t, const char **,
    const char **);

/*
 * End static declarations
 */

prom_metric_sample_histogram_t*
prom_metric_sample_histogram_new(const char *name,
        prom_histogram_buckets_t *buckets, size_t label_count,
        const char **label_keys, const char **label_values)
{
	prom_metric_sample_histogram_t *self;
	int error;

	/* Allocate and set self */
	self = malloc(sizeof(prom_metric_sample_histogram_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	/* Allocate and set the l_value_list */
	self->l_value_list = prom_linked_list_new();
	if (self->l_value_list == NULL)
		return NULL;

	/* Allocate and set the metric formatter */
	self->metric_formatter = prom_metric_formatter_new();
	if (self->metric_formatter == NULL) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Store map of l_value/prom_metric_sample_t */
	self->samples = prom_map_new();
	if (self->samples == NULL) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Set the free value function on the samples map */
	error = prom_map_set_free_value_fn(self->samples,
	    &prom_metric_sample_free_generic);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Set a map of bucket: l_value */
	self->l_values = prom_map_new();  /* Store map of bucket/l_value */
	if (self->l_values == NULL) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Set the free value function for the l_values map */
	error = prom_map_set_free_value_fn(self->l_values,
	    prom_metric_sample_histogram_free_str_generic);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	self->buckets = buckets;

	/* Allocate and initialize the lock */
	self->rwlock = malloc(sizeof(pthread_rwlock_t));
	if (self->rwlock == NULL) {
		prom_metric_sample_histogram_destroy(self);
		pr_enomem();
		return NULL;
	}

	error = pthread_rwlock_init(self->rwlock, NULL);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		pr_op_errno(error, "Calling pthread_rwlock_init");
		return NULL;
	}

	/* Allocate and initialize bucket metric samples */
	error = prom_metric_sample_histogram_init_bucket_samples(self, name,
	    label_count, label_keys, label_values);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Allocate and initialize the +Inf metric sample */
	error = prom_metric_sample_histogram_init_inf(self, name, label_count,
	    label_keys, label_values);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Allocate and initialize the count metric sample */
	error = prom_metric_sample_histogram_init_count(self, name, label_count,
	    label_keys, label_values);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/* Add summary sample */
	error = prom_metric_sample_histogram_init_summary(self, name,
	    label_count, label_keys, label_values);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	/*
	 * The value of nodes in this map will be simple prom_metric_sample
	 * pointers.
	 */
	error = prom_map_set_free_value_fn(self->samples,
	    &prom_metric_sample_free_generic);
	if (error) {
		prom_metric_sample_histogram_destroy(self);
		return NULL;
	}

	return self;
}

static int
prom_metric_sample_histogram_init_bucket_samples(
    prom_metric_sample_histogram_t *self, const char *name, size_t label_count,
    const char **label_keys, const char **label_values)
{
	prom_metric_sample_t *sample;
	const char *l_value;
	const char *bucket_key;
	char *tmp;
	int bucket_count;
	int i;
	int error;

	if (self == NULL)
		return -EINVAL;

	bucket_count = prom_histogram_buckets_count(self->buckets);
	/*
	 * For each bucket, create an prom_metric_sample_t with an appropriate
	 * l_value and default value of 0.0. The l_value will contain the metric
	 * name, user labels, and finally, the label and bucket value.
	 */
	for (i = 0; i < bucket_count; i++) {
		l_value = prom_metric_sample_histogram_l_value_for_bucket(self,
		    name, label_count, label_keys, label_values,
		    self->buckets->upper_bounds[i]);
		if (l_value == NULL)
			return -ENOENT;

		tmp = strdup(l_value);
		if (tmp == NULL)
			return pr_enomem();

		error = prom_linked_list_append(self->l_value_list, tmp);
		if (error) {
			free(tmp);
			return error;
		}

		bucket_key = prom_metric_sample_histogram_bucket_to_str(
		    self->buckets->upper_bounds[i]);
		if (bucket_key == NULL)
			return -ENOENT;

		error = prom_map_set(self->l_values, bucket_key,
		    (char *) l_value);
		if (error)
			return error;

		sample = prom_metric_sample_new(PROM_HISTOGRAM, l_value, 0.0);
		if (sample == NULL)
			return -EINVAL;

		error = prom_map_set(self->samples, l_value, sample);
		if (error)
			return error;

		free((void *) bucket_key);
	}

	return 0;
}

static int
prom_metric_sample_histogram_init_inf(prom_metric_sample_histogram_t *self,
    const char *name, size_t label_count, const char **label_keys,
    const char **label_values)
{
	prom_metric_sample_t *inf_sample;
	const char *inf_l_value;
	char *tmp;
	int error;

	if (self == NULL)
		return -EINVAL;

	inf_l_value = prom_metric_sample_histogram_l_value_for_inf(self, name,
	    label_count, label_keys, label_values);
	if (inf_l_value == NULL)
		return -ENOENT;

	tmp = strdup(inf_l_value);
	if (tmp == NULL)
		return pr_enomem();

	error = prom_linked_list_append(self->l_value_list, tmp);
	if (error) {
		free(tmp);
		return error;
	}

	error = prom_map_set(self->l_values, "+Inf", (char *)inf_l_value);
	if (error)
		return error;

	inf_sample = prom_metric_sample_new(PROM_HISTOGRAM, (char *)inf_l_value,
	    0.0);
	if (inf_sample == NULL)
		return pr_enomem();

	return prom_map_set(self->samples, inf_l_value, inf_sample);
}

static int
prom_metric_sample_histogram_init_count(prom_metric_sample_histogram_t *self,
    const char *name, size_t label_count, const char **label_keys,
    const char **label_values)
{
	prom_metric_sample_t *count_sample;
	const char *count_l_value;
	char *tmp;
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_metric_formatter_load_l_value(self->metric_formatter, name,
	    "count", label_count, label_keys, label_values);
	if (error)
		return error;

	count_l_value = prom_metric_formatter_dump(self->metric_formatter);
	if (count_l_value == NULL)
		return -ENOENT;

	tmp = strdup(count_l_value);
	if (tmp == NULL)
		return pr_enomem();

	error = prom_linked_list_append(self->l_value_list, tmp);
	if (error) {
		free(tmp);
		return error;
	}

	error = prom_map_set(self->l_values, "count", (char *) count_l_value);
	if (error)
		return error;

	count_sample = prom_metric_sample_new(PROM_HISTOGRAM, count_l_value,
	    0.0);
	if (count_sample == NULL)
		return -EINVAL;

	return prom_map_set(self->samples, count_l_value, count_sample);
}

static int
prom_metric_sample_histogram_init_summary(prom_metric_sample_histogram_t *self,
    const char *name, size_t label_count, const char **label_keys,
    const char **label_values)
{
	prom_metric_sample_t *sum_sample;
	const char *sum_l_value;
	char *tmp;
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_metric_formatter_load_l_value(self->metric_formatter, name,
	        "sum", label_count, label_keys, label_values);
	if (error)
		return error;

	sum_l_value = prom_metric_formatter_dump(self->metric_formatter);
	if (sum_l_value == NULL)
		return -EINVAL;

	tmp = strdup(sum_l_value);
	if (tmp == NULL)
		return pr_enomem();

	error = prom_linked_list_append(self->l_value_list, tmp);
	if (error) {
		free(tmp);
		return error;
	}

	error = prom_map_set(self->l_values, "sum", (char *) sum_l_value);
	if (error)
		return error;

	sum_sample = prom_metric_sample_new(PROM_HISTOGRAM, sum_l_value, 0.0);
	if (sum_sample == NULL)
		return -EINVAL;

	return prom_map_set(self->samples, sum_l_value, sum_sample);
}

int
prom_metric_sample_histogram_destroy(prom_metric_sample_histogram_t *self)
{
	int error;
	int ret;

	if (self == NULL)
		return 0;

	ret = 0;
	error = prom_linked_list_destroy(self->l_value_list);
	self->l_value_list = NULL;
	if (error)
		ret = error;

	error = prom_map_destroy(self->samples);
	if (error)
		ret = error;
	self->samples = NULL;

	error = prom_map_destroy(self->l_values);
	if (error)
		ret = error;
	self->l_values = NULL;

	error = prom_metric_formatter_destroy(self->metric_formatter);
	if (error)
		ret = error;
	self->metric_formatter = NULL;

	error = pthread_rwlock_destroy(self->rwlock);
	if (error)
		ret = error;

	free(self->rwlock);
	self->rwlock = NULL;

	free(self);
	self = NULL;
	return ret;
}

int
prom_metric_sample_histogram_destroy_generic(void *gen)
{
	prom_metric_sample_histogram_t *self;
	int error;

	self = (prom_metric_sample_histogram_t *) gen;
	error = prom_metric_sample_histogram_destroy(self);
	self = NULL;

	return error;
}

void
prom_metric_sample_histogram_free_generic(void *gen)
{
	prom_metric_sample_histogram_destroy(
	    (prom_metric_sample_histogram_t *) gen);
}

int
prom_metric_sample_histogram_observe(prom_metric_sample_histogram_t *self,
    double value)
{
	prom_metric_sample_t *sample;
	prom_metric_sample_t *inf_sample;
	prom_metric_sample_t *count_sample;
	prom_metric_sample_t *sum_sample;
	const char *bucket_key;
	const char *l_value;
	const char *inf_l_value;
	const char *count_l_value;
	const char *sum_l_value;
	int bucket_count;
	int i;
	int error;

	rwlock_write_lock(self->rwlock);

#define HANDLE_UNLOCK(r)		\
do {					\
	rwlock_unlock(self->rwlock);	\
	return r;			\
} while (0);

	/* Update the counter for the proper bucket if found */
	bucket_count = prom_histogram_buckets_count(self->buckets);
	for (i = (bucket_count - 1); i >= 0; i--) {
		if (value > self->buckets->upper_bounds[i])
			break;

		bucket_key = prom_metric_sample_histogram_bucket_to_str(
		    self->buckets->upper_bounds[i]);
		if (bucket_key == NULL)
			HANDLE_UNLOCK(-EINVAL)

		l_value = prom_map_get(self->l_values, bucket_key);
		if (l_value == NULL) {
			free((void *)bucket_key);
			HANDLE_UNLOCK(-ENOENT)
		}

		sample = prom_map_get(self->samples, l_value);
		if (sample == NULL) {
			free((void *)bucket_key);
			HANDLE_UNLOCK(-ENOENT)
		}

		free((void *)bucket_key);
		error = prom_metric_sample_add(sample, 1.0);
		if (error)
			HANDLE_UNLOCK(error)
	}

	/* Update the +Inf and count samples */
	inf_l_value = prom_map_get(self->l_values, "+Inf");
	if (inf_l_value == NULL)
		HANDLE_UNLOCK(-ENOENT)

	inf_sample = prom_map_get(self->samples, inf_l_value);
	if (inf_sample == NULL)
		HANDLE_UNLOCK(-ENOENT)

	error = prom_metric_sample_add(inf_sample, 1.0);
	if (error)
		HANDLE_UNLOCK(error)

	count_l_value = prom_map_get(self->l_values, "count");
	if (count_l_value == NULL)
		HANDLE_UNLOCK(-ENOENT)

	count_sample = prom_map_get(self->samples, count_l_value);
	if (count_sample == NULL)
		HANDLE_UNLOCK(-ENOENT)

	error = prom_metric_sample_add(count_sample, 1.0);
	if (error)
		HANDLE_UNLOCK(error)

	/* Update the sum sample */
	sum_l_value = prom_map_get(self->l_values, "sum");
	if (sum_l_value == NULL)
		HANDLE_UNLOCK(-ENOENT)

	sum_sample = prom_map_get(self->samples, sum_l_value);
	if (sum_sample == NULL)
		HANDLE_UNLOCK(-ENOENT)

	error = prom_metric_sample_add(sum_sample, value);
	HANDLE_UNLOCK(error);

	rwlock_unlock(self->rwlock);
	return 0;
#undef HANDLE_UNLOCK
}

static void
value_for_bucket_cleanup(const char **arr, size_t max)
{
	size_t i;

	for (i = 0; i < max; i++)
		free((char *)arr[i]);
	free(arr);
}

static const char *
prom_metric_sample_histogram_l_value_for_bucket(
    prom_metric_sample_histogram_t *self, const char *name, size_t label_count,
    const char **label_keys, const char **label_values, double bucket)
{
	const char **new_keys;
	const char **new_values;
	const char *ret;
	size_t i;
	int error;

	if (self == NULL)
		return NULL;

	/* Make new array to hold label_keys with label key */
	new_keys = malloc((label_count + 1) * sizeof(char *));
	for (i = 0; i < label_count; i++) {
		new_keys[i] = strdup(label_keys[i]);
		if (new_keys[i] == NULL) {
			value_for_bucket_cleanup(new_keys, i);
			pr_enomem();
			return NULL;
		}
	}

	new_keys[label_count] = strdup("le");
	if (new_keys[label_count] == NULL) {
		value_for_bucket_cleanup(new_keys, label_count);
		pr_enomem();
		return NULL;
	}

	/* Make new array to hold label_values with le label value */
	new_values = malloc((label_count + 1) * sizeof(char *));
	for (i = 0; i < label_count; i++) {
		new_values[i] = strdup(label_values[i]);
		if (new_values[i] == NULL) {
			value_for_bucket_cleanup(new_values, i);
			value_for_bucket_cleanup(new_keys, label_count + 1);
			pr_enomem();
			return NULL;
		}
	}

	new_values[label_count] = prom_metric_sample_histogram_bucket_to_str(
	    bucket);
	if (new_values[label_count] == NULL) {
		value_for_bucket_cleanup(new_values, label_count);
		value_for_bucket_cleanup(new_keys, label_count + 1);
		pr_enomem();
		return NULL;
	}

	error = prom_metric_formatter_load_l_value(self->metric_formatter, name,
	    NULL, label_count + 1, new_keys, new_values);
	if (error) {
		value_for_bucket_cleanup(new_values, label_count + 1);
		value_for_bucket_cleanup(new_keys, label_count + 1);
		return NULL;
	}
	ret = (const char *) prom_metric_formatter_dump(self->metric_formatter);
	value_for_bucket_cleanup(new_values, label_count + 1);
	value_for_bucket_cleanup(new_keys, label_count + 1);

	return ret;
}

static const char *
prom_metric_sample_histogram_l_value_for_inf(
    prom_metric_sample_histogram_t *self, const char *name, size_t label_count,
    const char **label_keys, const char **label_values)
{
	const char **new_keys;
	const char **new_values;
	const char *ret;
	size_t i;
	int error;

	if (self == NULL)
		return NULL;

	/* Make new array to hold label_keys with label key */
	new_keys = malloc((label_count + 1) * sizeof(char*));
	for (i = 0; i < label_count; i++) {
		new_keys[i] = strdup(label_keys[i]);
		if (new_keys[i] == NULL) {
			value_for_bucket_cleanup(new_keys, i);
			pr_enomem();
			return NULL;
		}
	}

	new_keys[label_count] = strdup("le");
	if (new_keys[label_count] == NULL) {
		value_for_bucket_cleanup(new_keys, label_count);
		pr_enomem();
		return NULL;
	}

	/* Make new array to hold label_values with label value */
	new_values = malloc((label_count + 1) * sizeof(char*));
	for (i = 0; i < label_count; i++) {
		new_values[i] = strdup(label_values[i]);
		if (new_values[i] == NULL) {
			value_for_bucket_cleanup(new_values, i);
			value_for_bucket_cleanup(new_keys, label_count + 1);
			pr_enomem();
			return NULL;
		}
	}

	new_values[label_count] = strdup("+Inf");
	if (new_values[label_count] == NULL) {
		value_for_bucket_cleanup(new_values, label_count);
		value_for_bucket_cleanup(new_keys, label_count + 1);
		pr_enomem();
		return NULL;
	}

	error = prom_metric_formatter_load_l_value(self->metric_formatter, name,
	    NULL, label_count + 1, new_keys, new_values);
	if (error) {
		value_for_bucket_cleanup(new_values, label_count + 1);
		value_for_bucket_cleanup(new_keys, label_count + 1);
		return NULL;
	}

	ret = (const char *)prom_metric_formatter_dump(self->metric_formatter);
	value_for_bucket_cleanup(new_values, label_count + 1);
	value_for_bucket_cleanup(new_keys, label_count + 1);

	return ret;
}

static void
prom_metric_sample_histogram_free_str_generic(void *gen)
{
	char *str;

	str = (char *)gen;
	free(str);
	str = NULL;
}

char *
prom_metric_sample_histogram_bucket_to_str(double bucket)
{
	char *buf;

	buf = (char *)malloc(sizeof(char) * 50);
	sprintf(buf, "%f", bucket);

	return buf;
}
