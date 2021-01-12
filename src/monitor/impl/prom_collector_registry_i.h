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

#ifndef SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_I_H
#define SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_I_H

#include "monitor/impl/prom_collector_registry_t.h"

int prom_collector_registry_enable_custom_process_metrics(
    prom_collector_registry_t *, const char *, const char *);

#endif /* SRC_MONITOR_IMPL_PROM_COLLECTOR_REGISTRY_I_H */
