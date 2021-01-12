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

#ifndef SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_T_H
#define SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_T_H

#include <pthread.h>
#include <stdbool.h>

/* Public */
#include "monitor/def/prom_collector_registry.h"

/* Private */
#include "monitor/impl/prom_map_t.h"
#include "monitor/impl/prom_metric_formatter_t.h"
#include "monitor/impl/prom_string_builder_t.h"

struct prom_collector_registry {
	const char *name;
	/* Disables the collection of process metrics */
	bool disable_process_metrics;
	/* Map of collectors keyed by name */
	prom_map_t *collectors;
	/* Enables string building */
	prom_string_builder_t *string_builder;
	/* Metric formatter for metric exposition on bridge call */
	prom_metric_formatter_t *metric_formatter;
	/* Mutex for safety against concurrent registration */
	pthread_rwlock_t *lock;
};

#endif /* SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_T_H */
