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

#ifndef SRC_MONITOR_IMPL_PROM_METRIC_I_H
#define SRC_MONITOR_IMPL_PROM_METRIC_I_H

/* Private */
#include "monitor/impl/prom_metric_sample_histogram_t.h"
#include "monitor/impl/prom_metric_t.h"

/*
 * @brief API PRIVATE Returns a *prom_metric
 */
prom_metric_t *prom_metric_new(prom_metric_type_t, const char *, const char *,
    size_t, const char **);

/*
 * @brief API PRIVATE Destroys a *prom_metric
 */
int prom_metric_destroy(prom_metric_t *);

/*
 * @brief API PRIVATE takes a generic item, casts to a *prom_metric_t and
 * destroys it
 */
int prom_metric_destroy_generic(void *);

/*
 * @brief API Private takes a generic item, casts to a *prom_metric_t and
 * destroys it. Discards any errors.
 */
void prom_metric_free_generic(void *);

#endif  /* SRC_MONITOR_IMPL_PROM_METRIC_I_H */
