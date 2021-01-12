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
#include "monitor/impl/prom_map_i.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_linked_list_i.h"
#include "monitor/impl/prom_linked_list_t.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_map_t.h"

#define PROM_MAP_INITIAL_SIZE 32

static void
destroy_map_node_value_no_op(void *value)
{
}

/*
 * prom_map_node
 */

prom_map_node_t *
prom_map_node_new(const char *key, void *value,
    prom_map_node_free_value_fn free_value_fn)
{
	prom_map_node_t *self;

	self = malloc(sizeof(prom_map_node_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->key = strdup(key);
	if (self->key == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}
	self->value = value;
	self->free_value_fn = free_value_fn;

	return self;
}

int
prom_map_node_destroy(prom_map_node_t *self)
{
	if (self == NULL)
		return 0;

	free((void *)self->key);
	self->key = NULL;
	if (self->value != NULL)
		(*self->free_value_fn)(self->value);
	self->value = NULL;
	free(self);
	self = NULL;

	return 0;
}

void
prom_map_node_free(void *item)
{
	prom_map_node_destroy((prom_map_node_t *) item);
}

prom_linked_list_compare_t
prom_map_node_compare(void *item_a, void *item_b)
{
	prom_map_node_t *map_node_a;
	prom_map_node_t *map_node_b;

	map_node_a = (prom_map_node_t *) item_a;
	map_node_b = (prom_map_node_t *) item_b;

	return strcmp(map_node_a->key, map_node_b->key);
}

/*
 * prom_map
 */

prom_map_t *
prom_map_new(void)
{
	prom_map_t *self;
	int i;
	int error;

	self = malloc(sizeof(prom_map_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}
	self->size = 0;
	self->max_size = PROM_MAP_INITIAL_SIZE;

	self->keys = prom_linked_list_new();
	if (self->keys == NULL) {
		free(self);
		return NULL;
	}

	/*
	 * These each key will be allocated once by prom_map_node_new and used
	 * here as well to save memory. With that said we will only have to
	 * deallocate each key once. That will happen on prom_map_node_destroy.
	 */
	error = prom_linked_list_set_free_fn(self->keys,
	    prom_linked_list_no_op_free);
	if (error) {
		prom_linked_list_destroy(self->keys);
		free(self);
		return NULL;
	}

	self->addrs = malloc(sizeof(prom_linked_list_t) * self->max_size);
	if (self->addrs == NULL) {
		prom_linked_list_destroy(self->keys);
		free(self);
		pr_enomem();
		return NULL;
	}
	self->free_value_fn = destroy_map_node_value_no_op;

	for (i = 0; i < self->max_size; i++) {
		self->addrs[i] = prom_linked_list_new();
		error = prom_linked_list_set_free_fn(self->addrs[i],
		    prom_map_node_free);
		if (error) {
			prom_map_destroy(self);
			return NULL;
		}
		error = prom_linked_list_set_compare_fn(self->addrs[i],
		        prom_map_node_compare);
		if (error) {
			prom_map_destroy(self);
			return NULL;
		}
	}

	self->rwlock = malloc(sizeof(pthread_rwlock_t));
	if (self->rwlock == NULL) {
		prom_map_destroy(self);
		pr_enomem();
		return NULL;
	}

	error = pthread_rwlock_init(self->rwlock, NULL);
	if (error) {
		prom_map_destroy(self);
		pr_op_errno(error, PROM_PTHREAD_RWLOCK_INIT_ERROR);
		return NULL;
	}

	return self;
}

int
prom_map_destroy(prom_map_t *self)
{
	size_t i;
	int error;
	int ret;

	if (self == NULL)
		return 0;

	ret = 0;
	error = prom_linked_list_destroy(self->keys);
	if (error)
		ret = error;
	self->keys = NULL;

	for (i = 0; i < self->max_size; i++) {
		error = prom_linked_list_destroy(self->addrs[i]);
		if (error)
			ret = error;
		self->addrs[i] = NULL;
	}
	free(self->addrs);
	self->addrs = NULL;

	error = pthread_rwlock_destroy(self->rwlock);
	if (error) {
		pr_op_errno(error, PROM_PTHREAD_RWLOCK_DESTROY_ERROR);
		ret = error;
	}

	free(self->rwlock);
	self->rwlock = NULL;
	free(self);
	self = NULL;

	return ret;
}

static size_t
prom_map_get_index_internal(const char *key, size_t *size, size_t *max_size)
{
	size_t index;
	size_t a;
	size_t b;

	a = 31415;
	b = 27183;

	for (index = 0; *key != '\0'; key++, a = a * b % (*max_size - 1))
		index = (a * index + *key) % *max_size;

	return index;
}

/*
 * @brief API PRIVATE hash function that returns an array index from the given
 * key and prom_map.
 *
 * The algorithm is based off of Horner's method. In a simpler version, you set
 * the return value to 0. Next, for each character in the string, you add the
 * integer value of the current character to the product of the prime number and
 * the current return value, set the result to the return value, then finally
 * return the return value.
 *
 * In this version of the algorithm, we attempt to achieve a probably of key to
 * index conversion collisions to 1/M (with M being the max_size of the map).
 * This optimizes dispersion and consequently, evens out the performance for
 * gets and sets for each item. Instead of using a fixed prime number, we
 * generate a coefficient for each iteration through the loop.
 *
 * Reference:
 *   * Algorithms in C: Third Edition by Robert Sedgewick, p579
 */
size_t
prom_map_get_index(prom_map_t *self, const char *key)
{
	return prom_map_get_index_internal(key, &self->size, &self->max_size);
}

static void *
prom_map_get_internal(const char *key, size_t *size, size_t *max_size,
    prom_linked_list_t *keys, prom_linked_list_t **addrs,
    prom_map_node_free_value_fn free_value_fn)
{
	size_t index;
	prom_linked_list_t *list;
	prom_map_node_t *temp_map_node;
	prom_linked_list_node_t *current_node;
	prom_map_node_t *current_map_node;
	prom_linked_list_compare_t result;

	index = prom_map_get_index_internal(key, size, max_size);
	list = addrs[index];
	temp_map_node = prom_map_node_new(key, NULL, free_value_fn);
	if (temp_map_node == NULL)
		return NULL;

	for (current_node = list->head; current_node != NULL;
	    current_node = current_node->next) {
		current_map_node = (prom_map_node_t *) current_node->item;
		result = prom_linked_list_compare(list, current_map_node,
		    temp_map_node);
		if (result == PROM_EQUAL) {
			prom_map_node_destroy(temp_map_node);
			temp_map_node = NULL;
			return current_map_node->value;
		}
	}
	prom_map_node_destroy(temp_map_node);
	temp_map_node = NULL;
	return NULL;
}

void *
prom_map_get(prom_map_t *self, const char *key)
{
	void *payload;

	if (self == NULL)
		return NULL;

	rwlock_write_lock(self->rwlock);
	payload = prom_map_get_internal(key, &self->size, &self->max_size,
	    self->keys, self->addrs, self->free_value_fn);
	rwlock_unlock(self->rwlock);

	return payload;
}

static int
prom_map_set_internal(const char *key, void *value, size_t *size,
    size_t *max_size, prom_linked_list_t *keys, prom_linked_list_t **addrs,
    prom_map_node_free_value_fn free_value_fn, bool destroy_current_value)
{
	prom_map_node_t *map_node;
	prom_linked_list_t *list;
	prom_linked_list_node_t *current_node;
	prom_map_node_t *current_map_node;
	prom_linked_list_compare_t result;
	size_t index;

	map_node = prom_map_node_new(key, value, free_value_fn);
	if (map_node == NULL)
		return pr_enomem();

	index = prom_map_get_index_internal(key, size, max_size);
	list = addrs[index];
	for (current_node = list->head; current_node != NULL;
	    current_node = current_node->next) {
		current_map_node = (prom_map_node_t *) current_node->item;
		result = prom_linked_list_compare(list, current_map_node,
		    map_node);
		if (result == PROM_EQUAL) {
			if (destroy_current_value) {
				free_value_fn(current_map_node->value);
				current_map_node->value = NULL;
			}
			free((char *) current_map_node->key);
			current_map_node->key = NULL;
			free(current_map_node);
			current_map_node = NULL;
			current_node->item = map_node;
			return 0;
		}
	}
	prom_linked_list_append(list, map_node);
	prom_linked_list_append(keys, (char *) map_node->key);
	(*size)++;

	return 0;
}

static void
linked_list_arr_destroy(prom_linked_list_t **arr, size_t max)
{
	size_t i;

	for(i = 0; i < max; i++)
		prom_linked_list_destroy(arr[i]);
	free(arr);
}

int
prom_map_ensure_space(prom_map_t *self)
{
	prom_linked_list_t *new_keys;
	prom_linked_list_t **new_addrs;
	prom_linked_list_t *list;
	prom_linked_list_node_t *current_node;
	prom_linked_list_node_t *next;
	prom_map_node_t *map_node;
	size_t new_max;
	size_t new_size;
	int i;
	int error;

	if (self == NULL)
		return -EINVAL;

	if (self->size <= self->max_size / 2)
		return 0;

	/* Increase the max size */
	new_max = self->max_size * 2;
	new_size = 0;

	/* Create a new list of keys */
	new_keys = prom_linked_list_new();
	if (new_keys == NULL)
		return pr_enomem();

	error = prom_linked_list_set_free_fn(new_keys,
	    prom_linked_list_no_op_free);
	if (error) {
		prom_linked_list_destroy(new_keys);
		return error;
	}

	/* Create a new array of addrs */
	new_addrs = malloc(sizeof(prom_linked_list_t) * new_max);
	if (new_addrs == NULL) {
		prom_linked_list_destroy(new_keys);
		return pr_enomem();
	}

	/* Initialize the new array */
	for (i = 0; i < new_max; i++) {
		new_addrs[i] = prom_linked_list_new();
		if (new_addrs[i] == NULL) {
			linked_list_arr_destroy(new_addrs, i);
			prom_linked_list_destroy(new_keys);
			return pr_enomem();
		}
		error = prom_linked_list_set_free_fn(new_addrs[i],
		    prom_map_node_free);
		if (error) {
			linked_list_arr_destroy(new_addrs, i + 1);
			prom_linked_list_destroy(new_keys);
			return error;
		}
		error = prom_linked_list_set_compare_fn(new_addrs[i],
		    prom_map_node_compare);
		if (error) {
			linked_list_arr_destroy(new_addrs, i + 1);
			prom_linked_list_destroy(new_keys);
			return error;
		}
	}

	/*
	 * Iterate through each linked-list at each memory region in the map's
	 * backbone
	 */
	for (i = 0; i < self->max_size; i++) {
		/*
		 * Create a new map node for each node in the linked list and
		 * insert it into the new map. Afterwards, deallocate the old
		 * map node
		 */
		list = self->addrs[i];
		current_node = list->head;
		while (current_node != NULL) {
			map_node = (prom_map_node_t *) current_node->item;
			error = prom_map_set_internal(map_node->key,
			    map_node->value, &new_size, &new_max, new_keys,
			    new_addrs, self->free_value_fn, false);
			if (error) {
				linked_list_arr_destroy(new_addrs, new_max);
				prom_linked_list_destroy(new_keys);
				return error;
			}

			next = current_node->next;
			free(current_node);
			current_node = NULL;
			free((void *) map_node->key);
			map_node->key = NULL;
			free(map_node);
			map_node = NULL;
			current_node = next;
		}
		/*
		 * We're done deallocating each map node in the linked list, so
		 * deallocate the linked-list object
		 */
		free(self->addrs[i]);
		self->addrs[i] = NULL;
	}

	/* Destroy the collection of keys in the map */
	prom_linked_list_destroy(self->keys);
	self->keys = NULL;

	/* Deallocate the backbone of the map */
	free(self->addrs);
	self->addrs = NULL;

	/* Update the members of the current map */
	self->size = new_size;
	self->max_size = new_max;
	self->keys = new_keys;
	self->addrs = new_addrs;

	return 0;
}

int
prom_map_set(prom_map_t *self, const char *key, void *value)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	rwlock_write_lock(self->rwlock);

	error = prom_map_ensure_space(self);
	if (error) {
		rwlock_unlock(self->rwlock);
		return error;
	}

	error = prom_map_set_internal(key, value, &self->size, &self->max_size,
	    self->keys, self->addrs, self->free_value_fn, true);
	if (error) {
		rwlock_unlock(self->rwlock);
		return error;
	}
	rwlock_unlock(self->rwlock);

	return 0;
}

static int
prom_map_delete_internal(const char *key, size_t *size, size_t *max_size,
    prom_linked_list_t *keys, prom_linked_list_t **addrs,
    prom_map_node_free_value_fn free_value_fn)
{
	prom_linked_list_t *list;
	prom_linked_list_node_t *current_node;
	prom_map_node_t *temp_map_node;
	prom_map_node_t *current_map_node;
	prom_linked_list_compare_t result;
	size_t index;
	int error;

	index = prom_map_get_index_internal(key, size, max_size);
	list = addrs[index];
	temp_map_node = prom_map_node_new(key, NULL, free_value_fn);
	if (temp_map_node == NULL)
		return pr_enomem();

	for (current_node = list->head; current_node != NULL;
	    current_node = current_node->next) {
		current_map_node = (prom_map_node_t *) current_node->item;
		result = prom_linked_list_compare(list, current_map_node,
		    temp_map_node);
		if (result == PROM_EQUAL) {
			error = prom_linked_list_remove(list, current_node);
			if (error) {
				prom_map_node_destroy(temp_map_node);
				return error;
			}

			error = prom_linked_list_remove(keys,
			    (char *) current_map_node->key);
			if (error) {
				prom_map_node_destroy(temp_map_node);
				return error;
			}

			(*size)--;
			break;
		}
	}
	error = prom_map_node_destroy(temp_map_node);
	temp_map_node = NULL;

	return error;
}

int
prom_map_delete(prom_map_t *self, const char *key)
{
	int error;

	if (self == NULL)
		return -EINVAL;

	rwlock_write_lock(self->rwlock);
	error = prom_map_delete_internal(key, &self->size, &self->max_size,
	        self->keys, self->addrs, self->free_value_fn);
	rwlock_unlock(self->rwlock);

	return error;
}

int
prom_map_set_free_value_fn(prom_map_t *self,
    prom_map_node_free_value_fn free_value_fn)
{
	if (self == NULL)
		return -EINVAL;

	self->free_value_fn = free_value_fn;
	return 0;
}

size_t
prom_map_size(prom_map_t *self)
{
	return self->size;
}
