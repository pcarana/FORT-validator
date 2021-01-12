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

#ifndef SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_I_H
#define SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_I_H

/* Public */
#include "monitor/def/prom_metric_sample_histogram.h"

/* Private */
#include "monitor/impl/prom_metric_sample_histogram_t.h"

/*
 * @brief API PRIVATE Create a pointer to a prom_metric_sample_histogram_t
 */
prom_metric_sample_histogram_t *prom_metric_sample_histogram_new(const char *,
    prom_histogram_buckets_t *, size_t, const char **, const char **);

/*
 * @brief API PRIVATE Destroy a prom_metric_sample_histogram_t
 */
int prom_metric_sample_histogram_destroy(prom_metric_sample_histogram_t *);

/*
 * @brief API PRIVATE Destroy a void pointer that is cast to a
 * prom_metric_sample_histogram_t*
 */
int prom_metric_sample_histogram_destroy_generic(void *);

char *prom_metric_sample_histogram_bucket_to_str(double);

void prom_metric_sample_histogram_free_generic(void *);

#endif /* SRC_MONITOR_IMPL_PROM_METRIC_SAMPLE_HISTOGRAM_I_H */
