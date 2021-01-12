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

#ifndef SRC_MONITOR_IMPL_PROM_PROCESS_LIMITS_T_H
#define SRC_MONITOR_IMPL_PROM_PROCESS_LIMITS_T_H

#include "monitor/def/prom_gauge.h"
#include "monitor/impl/prom_procfs_t.h"

extern prom_gauge_t *prom_process_open_fds;
extern prom_gauge_t *prom_process_max_fds;
extern prom_gauge_t *prom_process_virtual_memory_max_bytes;
extern prom_gauge_t *prom_process_resident_memory_bytes;

typedef struct prom_process_limits_row {
	/* Pointer to a string */
	const char *limit;
	/* Soft value */
	int soft;
	/* Hard value */
	int hard;
	/* Units */
	const char *units;
} prom_process_limits_row_t;

typedef struct prom_process_limits_current_row {
	/* Pointer to a string */
	char *limit;
	/* Soft value */
	int soft;
	/* Hard value */
	int hard;
	/* Units */
	char *units;
} prom_process_limits_current_row_t;

typedef prom_procfs_buf_t prom_process_limits_file_t;

#endif /* SRC_MONITOR_IMPL_PROM_PROCESS_LIMITS_T_H */
