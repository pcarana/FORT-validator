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

#include "monitor/def/prom_collector_registry.h"

#include <errno.h>
#include <pthread.h>
/* FIXME (later) This isn't portable */
/* #include <regex.h> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* Public */
#include "monitor/def/prom_collector.h"

/* Private */
#include "monitor/impl/prom_collector_registry_t.h"
#include "monitor/impl/prom_collector_t.h"
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_metric_formatter_i.h"
#include "monitor/impl/prom_metric_i.h"
#include "monitor/impl/prom_metric_t.h"
#include "monitor/impl/prom_process_limits_i.h"
#include "monitor/impl/prom_string_builder_i.h"

prom_collector_registry_t *PROM_COLLECTOR_REGISTRY_DEFAULT;

prom_collector_registry_t *
prom_collector_registry_new(const char *name)
{
	prom_collector_registry_t *self;
	int error;

	self = malloc(sizeof(prom_collector_registry_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->disable_process_metrics = false;

	self->name = strdup(name);
	if (self->name == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	self->collectors = prom_map_new();
	if (self->collectors == NULL) {
		prom_collector_registry_destroy(self);
		return NULL;
	}

	error = prom_map_set_free_value_fn(self->collectors,
	    &prom_collector_free_generic);
	if (error) {
		prom_collector_registry_destroy(self);
		return NULL;
	}

	error = prom_map_set(self->collectors, "default",
	    prom_collector_new("default"));
	if (error) {
		prom_collector_registry_destroy(self);
		return NULL;
	}

	self->metric_formatter = prom_metric_formatter_new();
	if (self->metric_formatter == NULL) {
		prom_collector_registry_destroy(self);
		return NULL;
	}

	self->string_builder = prom_string_builder_new();
	if (self->string_builder == NULL) {
		prom_collector_registry_destroy(self);
		return NULL;
	}

	self->lock = malloc(sizeof(pthread_rwlock_t));
	if (self->lock == NULL) {
		prom_collector_registry_destroy(self);
		pr_enomem();
		return NULL;
	}

	error = pthread_rwlock_init(self->lock, NULL);
	if (error) {
		pr_op_errno(error, "failed to initialize rwlock");
		prom_collector_registry_destroy(self);
		return NULL;
	}

	return self;
}

int
prom_collector_registry_enable_process_metrics(prom_collector_registry_t *self)
{
	prom_collector_t *process_collector;

	if (self == NULL)
		return -EINVAL;

	process_collector = prom_collector_process_new(NULL, NULL);
	if (process_collector == NULL)
		return -EINVAL;

	return prom_map_set(self->collectors, "process", process_collector);
}

int
prom_collector_registry_enable_custom_process_metrics(
    prom_collector_registry_t *self, const char *process_limits_path,
    const char *process_stats_path)
{
	prom_collector_t *process_collector;

	if (self == NULL) {
		PROM_LOG("prom_collector_registry_t is NULL");
		return -EINVAL;
	}

	process_collector = prom_collector_process_new(process_limits_path,
	    process_stats_path);
	if (process_collector == NULL)
		return pr_enomem();

	prom_map_set(self->collectors, "process", process_collector);

	return 0;
}

int
prom_collector_registry_default_init(void)
{
	if (PROM_COLLECTOR_REGISTRY_DEFAULT != NULL)
		return 0;

	PROM_COLLECTOR_REGISTRY_DEFAULT =
	    prom_collector_registry_new("default");
	if (PROM_COLLECTOR_REGISTRY_DEFAULT == NULL)
		return -EINVAL;

	return prom_collector_registry_enable_process_metrics(
	    PROM_COLLECTOR_REGISTRY_DEFAULT);
}

int
prom_collector_registry_destroy(prom_collector_registry_t *self)
{
	int error;
	int ret;

	ret = 0;
	if (self == NULL)
		return 0;

	error = prom_map_destroy(self->collectors);
	self->collectors = NULL;
	if (error)
		ret = error;

	error = prom_metric_formatter_destroy(self->metric_formatter);
	self->metric_formatter = NULL;
	if (error)
		ret = error;

	error = prom_string_builder_destroy(self->string_builder);
	self->string_builder = NULL;
	if (error)
		ret = error;

	error = pthread_rwlock_destroy(self->lock);
	free(self->lock);
	self->lock = NULL;
	if (error)
		ret = error;

	free((char *) self->name);
	self->name = NULL;

	free(self);
	self = NULL;

	return ret;
}

int
prom_collector_registry_register_metric(prom_metric_t *metric)
{
	prom_collector_t *default_collector;

	default_collector = (prom_collector_t *)prom_map_get(
	    PROM_COLLECTOR_REGISTRY_DEFAULT->collectors,
	    "default");

	if (default_collector == NULL)
		return -ENOENT;

	return prom_collector_add_metric(default_collector, metric);
}

prom_metric_t *
prom_collector_registry_must_register_metric(prom_metric_t *metric)
{
	int error;

	error = prom_collector_registry_register_metric(metric);
	if (error) {
		pr_op_err("prom_collector_registry_register_metric() error'd [code=%d]",
		    error);
		return NULL;
	}

	return metric;
}

int
prom_collector_registry_register_collector(prom_collector_registry_t *self,
    prom_collector_t *collector)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	rwlock_write_lock(self->lock);

	if (prom_map_get(self->collectors, collector->name) != NULL) {
		rwlock_unlock(self->lock);
		return -EEXIST;
	}

	error = prom_map_set(self->collectors, collector->name, collector);
	if (error) {
		rwlock_unlock(self->lock);
		return error;
	}

	rwlock_unlock(self->lock);

	return 0;
}

int
prom_collector_registry_validate_metric_name(prom_collector_registry_t *self,
    const char *metric_name)
{
	/* FIXME(later) This isn't portable */
	/*
	regex_t r;
	int ret = 0;
	ret = regcomp(&r, "^[a-zA-Z_:][a-zA-Z0-9_:]*$", REG_EXTENDED);
	if (ret) {
		PROM_LOG(PROM_REGEX_REGCOMP_ERROR);
		regfree(&r);
		return ret;
	}

	ret = regexec(&r, metric_name, 0, NULL, 0);
	if (ret) {
		PROM_LOG(PROM_REGEX_REGEXEC_ERROR);
		regfree(&r);
		return ret;
	}
	regfree(&r);
	*/
	return 0;
}

const char *
prom_collector_registry_bridge(prom_collector_registry_t *self)
{
	prom_metric_formatter_clear(self->metric_formatter);
	prom_metric_formatter_load_metrics(self->metric_formatter,
	    self->collectors);

	return (const char *) prom_metric_formatter_dump(
	    self->metric_formatter);
}
