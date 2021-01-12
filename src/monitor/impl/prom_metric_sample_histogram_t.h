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

#ifndef SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_T_H
#define SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_T_H

#include <pthread.h>

/* Public */
#include "monitor/def/prom_histogram_buckets.h"
#include "monitor/def/prom_metric_sample_histogram.h"

/* Private */
#include "monitor/impl/prom_map_t.h"
#include "monitor/impl/prom_metric_formatter_t.h"

struct prom_metric_sample_histogram {
	prom_linked_list_t *l_value_list;
	prom_map_t *l_values;
	prom_map_t *samples;
	prom_metric_formatter_t *metric_formatter;
	prom_histogram_buckets_t *buckets;
	pthread_rwlock_t *rwlock;
};

#endif /* SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_T_H */
