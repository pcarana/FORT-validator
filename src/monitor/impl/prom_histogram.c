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
#include "monitor/def/prom_histogram.h"

#include <errno.h>

/* Public */
#include "monitor/def/prom_histogram_buckets.h"

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_metric_i.h"
#include "monitor/impl/prom_metric_sample_histogram_i.h"
#include "monitor/impl/prom_metric_sample_histogram_t.h"
#include "monitor/impl/prom_metric_t.h"

prom_histogram_t *
prom_histogram_new(const char *name, const char *help,
    prom_histogram_buckets_t *buckets, size_t label_key_count,
    const char **label_keys)
{
	prom_histogram_t *self;
	int i;

	self = (prom_histogram_t *)prom_metric_new(PROM_HISTOGRAM, name, help,
	    label_key_count, label_keys);
	if (self == NULL)
		return NULL;

	if (buckets == NULL) {
		self->buckets = prom_histogram_default_buckets;
		return self;
	}

	/* Ensure the bucket values are increasing */
	for (i = 1; i < buckets->count; i++) {
		if (buckets->upper_bounds[i - 1] > buckets->upper_bounds[i]) {
			prom_histogram_destroy(self);
			return NULL;
		}
	}
	self->buckets = buckets;

	return self;
}

int
prom_histogram_destroy(prom_histogram_t *self)
{
	int error;

	if (self == NULL)
		return 0;

	error = prom_metric_destroy(self);
	if (error)
		return error;
	self = NULL;

	return 0;
}

int
prom_histogram_observe(prom_histogram_t *self, double value,
    const char **label_values)
{
	prom_metric_sample_histogram_t *h_sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_HISTOGRAM) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return 1;
	}

	h_sample = prom_metric_sample_histogram_from_labels(self, label_values);
	if (h_sample == NULL)
		return -ENOENT;

	return prom_metric_sample_histogram_observe(h_sample, value);
}
