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

#ifndef SRC_MONITOR_IMPL_PROM_METRIC_T_H
#define SRC_MONITOR_IMPL_PROM_METRIC_T_H

#include <pthread.h>

/* Public */
#include "monitor/def/prom_histogram_buckets.h"
#include "monitor/def/prom_metric.h"

/* Private */
#include "monitor/impl/prom_map_i.h"
#include "monitor/impl/prom_map_t.h"
#include "monitor/impl/prom_metric_formatter_t.h"

/*
 * @brief API PRIVATE Contains metric type constants
 */
typedef enum prom_metric_type {
	PROM_COUNTER,
	PROM_GAUGE,
	PROM_HISTOGRAM,
	PROM_SUMMARY,
} prom_metric_type_t;

/*
 * @brief API PRIVATE Maps metric type constants to human readable string values
 */
extern char *prom_metric_type_map[4];

/*
 * @brief API PRIVATE An opaque struct to users containing metric metadata; one
 *        or more metric samples; and a metric formatter for locating metric
 *        samples and exporting metric data
 */
struct prom_metric {
	/* The type of metric */
	prom_metric_type_t type;
	/* The name of the metric */
	const char *name;
	/* The help output for the metric */
	const char *help;
	/* Map comprised of samples for the given metric */
	prom_map_t *samples;
	/* Array of histogram bucket upper bound values */
	prom_histogram_buckets_t *buckets;
	/* The count of labe_keys */
	size_t label_key_count;
	/* The metric formatter */
	prom_metric_formatter_t *formatter;
	/* Required for locking on certain non-atomic operations */
	pthread_rwlock_t *rwlock;
	/* Array comprised of const char */
	const char **label_keys;
};

#endif /* SRC_MONITOR_IMPL_PROM_METRIC_T_H */
