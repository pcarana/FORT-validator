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

#ifndef SRC_MONITOR_IMPL_PROM_PROCFS_I_H
#define SRC_MONITOR_IMPL_PROM_PROCFS_I_H

#include "monitor/impl/prom_procfs_t.h"

prom_procfs_buf_t *prom_procfs_buf_new(const char *);

int prom_procfs_buf_destroy(prom_procfs_buf_t *);

#endif /* SRC_MONITOR_IMPL_PROM_PROCFS_I_H */
