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
#include "monitor/impl/prom_metric_formatter_i.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

/* Private */
#include "monitor/impl/prom_collector_t.h"
#include "monitor/impl/prom_linked_list_t.h"
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_metric_sample_histogram_t.h"
#include "monitor/impl/prom_metric_sample_t.h"
#include "monitor/impl/prom_metric_t.h"
#include "monitor/impl/prom_string_builder_i.h"

prom_metric_formatter_t *
prom_metric_formatter_new(void)
{
	prom_metric_formatter_t *self;

	self = malloc(sizeof(prom_metric_formatter_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->string_builder = prom_string_builder_new();
	if (self->string_builder == NULL) {
		free(self);
		return NULL;
	}

	self->err_builder = prom_string_builder_new();
	if (self->err_builder == NULL) {
		prom_string_builder_destroy(self->string_builder);
		free(self);
		return NULL;
	}

	return self;
}

int
prom_metric_formatter_destroy(prom_metric_formatter_t *self)
{
	int error;
	int ret;

	if (self == NULL)
		return 0;

	ret = 0;

	error = prom_string_builder_destroy(self->string_builder);
	self->string_builder = NULL;
	if (error)
		ret = error;

	error = prom_string_builder_destroy(self->err_builder);
	self->err_builder = NULL;
	if (error)
		ret = error;

	free(self);
	self = NULL;

	return ret;
}

int
prom_metric_formatter_load_help(prom_metric_formatter_t *self, const char *name,
    const char *help)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_string_builder_add_str(self->string_builder, "# HELP ");
	if (error)
		return error;

	error = prom_string_builder_add_str(self->string_builder, name);
	if (error)
		return error;

	error = prom_string_builder_add_char(self->string_builder, ' ');
	if (error)
		return error;

	error = prom_string_builder_add_str(self->string_builder, help);
	if (error)
		return error;

	return prom_string_builder_add_char(self->string_builder, '\n');
}

int
prom_metric_formatter_load_type(prom_metric_formatter_t *self, const char *name,
    prom_metric_type_t metric_type)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_string_builder_add_str(self->string_builder, "# TYPE ");
	if (error)
		return error;

	error = prom_string_builder_add_str(self->string_builder, name);
	if (error)
		return error;

	error = prom_string_builder_add_char(self->string_builder, ' ');
	if (error)
		return error;

	error = prom_string_builder_add_str(self->string_builder,
	    prom_metric_type_map[metric_type]);
	if (error)
		return error;

	return prom_string_builder_add_char(self->string_builder, '\n');
}

int
prom_metric_formatter_load_l_value(prom_metric_formatter_t *self,
    const char *name, const char *suffix, size_t label_count,
    const char **label_keys, const char **label_values)
{
	int i;
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_string_builder_add_str(self->string_builder, name);
	if (error)
		return error;

	if (suffix != NULL) {
		error = prom_string_builder_add_char(self->string_builder, '_');
		if (error)
			return error;

		error = prom_string_builder_add_str(self->string_builder,
		    suffix);
		if (error)
			return error;
	}

	if (label_count == 0)
		return 0;

	for (i = 0; i < label_count; i++) {
		if (i == 0) {
			error = prom_string_builder_add_char(
			    self->string_builder, '{');
			if (error)
				return error;
		}
		error = prom_string_builder_add_str(self->string_builder,
		    (const char *) label_keys[i]);
		if (error)
			return error;

		error = prom_string_builder_add_char(self->string_builder, '=');
		if (error)
			return error;

		error = prom_string_builder_add_char(self->string_builder, '"');
		if (error)
			return error;

		error = prom_string_builder_add_str(self->string_builder,
		        (const char*) label_values[i]);
		if (error)
			return error;

		error = prom_string_builder_add_char(self->string_builder, '"');
		if (error)
			return error;

		if (i == label_count - 1) {
			error = prom_string_builder_add_char(
			    self->string_builder, '}');
			if (error)
				return error;
		} else {
			error = prom_string_builder_add_char(
			    self->string_builder, ',');
			if (error)
				return error;
		}
	}

	return 0;
}

int
prom_metric_formatter_load_sample(prom_metric_formatter_t *self,
    prom_metric_sample_t *sample)
{
	char buffer[50];
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_string_builder_add_str(self->string_builder,
	    sample->l_value);
	if (error)
		return error;

	error = prom_string_builder_add_char(self->string_builder, ' ');
	if (error)
		return error;

	sprintf(buffer, "%f", sample->r_value);
	error = prom_string_builder_add_str(self->string_builder, buffer);
	if (error)
		return error;

	return prom_string_builder_add_char(self->string_builder, '\n');
}

int
prom_metric_formatter_clear(prom_metric_formatter_t *self)
{
	if (self == NULL)
		return -EINVAL;

	return prom_string_builder_clear(self->string_builder);
}

char *
prom_metric_formatter_dump(prom_metric_formatter_t *self)
{
	char *data;
	int error;

	if (self == NULL)
			return NULL;

	data = prom_string_builder_dump(self->string_builder);
	if (data == NULL)
		return NULL;

	error = prom_string_builder_clear(self->string_builder);
	if (error) {
		free(data);
		return NULL;
	}

	return data;
}

int
prom_metric_formatter_load_metric(prom_metric_formatter_t *self,
    prom_metric_t *metric)
{
	prom_linked_list_node_t *current_node;
	prom_linked_list_node_t *current_hist_node;
	prom_metric_sample_t *sample;
	prom_metric_sample_histogram_t *hist_sample;
	const char *key;
	const char *hist_key;
	int error;

	if (self == NULL)
		return -EINVAL;

	error = prom_metric_formatter_load_help(self, metric->name,
	    metric->help);
	if (error)
		return error;

	error = prom_metric_formatter_load_type(self, metric->name,
	    metric->type);
	if (error)
		return error;

	for (current_node = metric->samples->keys->head; current_node != NULL;
	    current_node = current_node->next) {
		key = (const char *) current_node->item;
		if (metric->type != PROM_HISTOGRAM) {
			sample = (prom_metric_sample_t *) prom_map_get(
			    metric->samples, key);
			if (sample == NULL)
				return -ENOENT;

			error = prom_metric_formatter_load_sample(self, sample);
			if (error)
				return error;
			continue;
		}

		hist_sample = (prom_metric_sample_histogram_t *) prom_map_get(
		    metric->samples, key);
		if (hist_sample == NULL)
			return -ENOENT;

		for (current_hist_node = hist_sample->l_value_list->head;
		    current_hist_node != NULL;
		    current_hist_node = current_hist_node->next) {
			hist_key = (const char *) current_hist_node->item;
			sample = (prom_metric_sample_t *) prom_map_get(
			    hist_sample->samples, hist_key);
			if (sample == NULL)
				return -ENOENT;
			error = prom_metric_formatter_load_sample(self, sample);
			if (error)
				return error;
		}
	}

	return prom_string_builder_add_char(self->string_builder, '\n');
}

int
prom_metric_formatter_load_metrics(prom_metric_formatter_t *self,
    prom_map_t *collectors)
{
	prom_linked_list_node_t *current_node;
	prom_linked_list_node_t *metrics_node;
	prom_collector_t *collector;
	prom_map_t *metrics;
	prom_metric_t *metric;
	const char *collector_name;
	const char *metric_name;
	int error;

	if (self == NULL)
		return -EINVAL;

	for (current_node = collectors->keys->head; current_node != NULL;
	    current_node = current_node->next) {
		collector_name = (const char *) current_node->item;
		collector = (prom_collector_t *) prom_map_get(collectors,
		    collector_name);
		if (collector == NULL)
			return -ENOENT;

		metrics = collector->collect_fn(collector);
		if (metrics == NULL)
			return -ENOENT;

		for (metrics_node = metrics->keys->head; metrics_node != NULL;
		    metrics_node = metrics_node->next) {
			metric_name = (const char *) metrics_node->item;
			metric = (prom_metric_t *) prom_map_get(metrics,
			    metric_name);
			if (metric == NULL)
				return -ENOENT;
			error = prom_metric_formatter_load_metric(self, metric);
			if (error)
				return error;
		}
	}

	return 0;
}
