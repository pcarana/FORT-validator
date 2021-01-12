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
#include "monitor/def/prom_gauge.h"

#include <errno.h>

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_metric_i.h"
#include "monitor/impl/prom_metric_sample_i.h"
#include "monitor/impl/prom_metric_sample_t.h"
#include "monitor/impl/prom_metric_t.h"

prom_gauge_t *
prom_gauge_new(const char *name, const char *help, size_t label_key_count,
    const char **label_keys)
{
	return (prom_gauge_t *)prom_metric_new(PROM_GAUGE, name, help,
	    label_key_count, label_keys);
}

int
prom_gauge_destroy(prom_gauge_t *self)
{
	int error;

	error = prom_metric_destroy(self);
	self = NULL;

	return error;
}

int
prom_gauge_inc(prom_gauge_t *self, const char **label_values)
{
	prom_metric_sample_t *sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_GAUGE) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return -EINVAL;
	}

	sample = prom_metric_sample_from_labels(self, label_values);
	if (sample == NULL)
		return -ENOENT;

	return prom_metric_sample_add(sample, 1.0);
}

int
prom_gauge_dec(prom_gauge_t *self, const char **label_values)
{
	prom_metric_sample_t *sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_GAUGE) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return -EINVAL;
	}

	sample = prom_metric_sample_from_labels(self, label_values);
	if (sample == NULL)
		return -ENOENT;

	return prom_metric_sample_sub(sample, 1.0);
}

int
prom_gauge_add(prom_gauge_t *self, double r_value, const char **label_values)
{
	prom_metric_sample_t *sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_GAUGE) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return -EINVAL;
	}

	sample = prom_metric_sample_from_labels(self, label_values);
	if (sample == NULL)
		return -ENOENT;

	return prom_metric_sample_add(sample, r_value);
}

int
prom_gauge_sub(prom_gauge_t *self, double r_value, const char **label_values)
{
	prom_metric_sample_t *sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_GAUGE) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return -EINVAL;
	}

	sample = prom_metric_sample_from_labels(self, label_values);
	if (sample == NULL)
		return -ENOENT;

	return prom_metric_sample_sub(sample, r_value);
}

int
prom_gauge_set(prom_gauge_t *self, double r_value, const char **label_values)
{
	prom_metric_sample_t *sample;

	if (self == NULL)
		return -EINVAL;
	if (self->type != PROM_GAUGE) {
		PROM_LOG(PROM_METRIC_INCORRECT_TYPE);
		return -EINVAL;
	}

	sample = prom_metric_sample_from_labels(self, label_values);
	if (sample == NULL)
		return -ENOENT;

	return prom_metric_sample_set(sample, r_value);
}
