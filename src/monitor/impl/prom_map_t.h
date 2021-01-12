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

#ifndef SRC_MONITOR_IMPL_PROM_MAP_T_H
#define SRC_MONITOR_IMPL_PROM_MAP_T_H

#include <pthread.h>

/* Public */
#include "monitor/def/prom_map.h"

/* Private */
#include "monitor/impl/prom_linked_list_t.h"

typedef void (*prom_map_node_free_value_fn)(void *);

struct prom_map_node {
	const char *key;
	void *value;
	prom_map_node_free_value_fn free_value_fn;
};

struct prom_map {
	/* Contains the size of the map */
	size_t size;
	/* Stores the current max_size */
	size_t max_size;
	/* Linked list containing containing all keys present */
	prom_linked_list_t *keys;
	/*
	 * Sequence of linked lists. Each list contains nodes with the same
	 * index
	 */
	prom_linked_list_t **addrs;
	pthread_rwlock_t *rwlock;
	prom_map_node_free_value_fn free_value_fn;
};

#endif /* SRC_MONITOR_IMPL_PROM_MAP_T_H */
