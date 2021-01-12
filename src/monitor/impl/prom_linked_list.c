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
#include "monitor/impl/prom_linked_list_i.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Private */
#include "monitor/impl/prom_linked_list_t.h"
#include "monitor/impl/prom_log.h"

prom_linked_list_t *
prom_linked_list_new(void)
{
	prom_linked_list_t *self;

	self = malloc(sizeof(prom_linked_list_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->head = NULL;
	self->tail = NULL;
	self->free_fn = NULL;
	self->compare_fn = NULL;
	self->size = 0;

	return self;
}

int
prom_linked_list_purge(prom_linked_list_t *self)
{
	prom_linked_list_node_t *node;
	prom_linked_list_node_t *next;

	if (self == NULL)
		return 0;

	node = self->head;
	while (node != NULL) {
		next = node->next;
		if (node->item != NULL) {
			if (self->free_fn) {
				(*self->free_fn)(node->item);
			} else {
				free(node->item);
			}
		}
		free(node);
		node = NULL;
		node = next;
	}
	self->head = NULL;
	self->tail = NULL;
	self->size = 0;

	return 0;
}

int
prom_linked_list_destroy(prom_linked_list_t *self)
{
	int error;

	error = prom_linked_list_purge(self);
	if (error)
		return error;

	free(self);
	self = NULL;

	return 0;
}

void *
prom_linked_list_first(prom_linked_list_t *self)
{
	if (self == NULL)
		return NULL;
	if (self->head)
		return self->head->item;

	return NULL;
}

void *
prom_linked_list_last(prom_linked_list_t *self)
{
	if (self == NULL)
		return NULL;
	if (self->tail)
		return self->tail->item;

	return NULL;
}

int
prom_linked_list_append(prom_linked_list_t *self, void *item)
{
	prom_linked_list_node_t *node;

	if (self == NULL)
		return -EINVAL;

	node = malloc(sizeof(prom_linked_list_node_t));
	if (node == NULL)
		return pr_enomem();

	node->item = item;
	if (self->tail)
		self->tail->next = node;
	else
		self->head = node;

	self->tail = node;
	node->next = NULL;
	self->size++;

	return 0;
}

int
prom_linked_list_push(prom_linked_list_t *self, void *item)
{
	prom_linked_list_node_t *node;

	if (self == NULL)
		return -EINVAL;

	node = malloc(sizeof(prom_linked_list_node_t));
	if (node == NULL)
		return pr_enomem();

	node->item = item;
	node->next = self->head;
	self->head = node;
	if (self->tail == NULL)
		self->tail = node;
	self->size++;

	return 0;
}

void *
prom_linked_list_pop(prom_linked_list_t *self)
{
	prom_linked_list_node_t *node;
	void *item;

	if (self == NULL)
		return NULL;

	node = self->head;
	item = NULL;
	if (node != NULL) {
		item = node->item;
		self->head = node->next;
		if (self->tail == node)
			self->tail = NULL;

		if (node->item != NULL) {
			if (self->free_fn) {
				(*self->free_fn)(node->item);
			} else {
				free(node->item);
			}
		}
		node->item = NULL;
		node = NULL;
		self->size--;
	}
	return item;
}

int
prom_linked_list_remove(prom_linked_list_t *self, void *item)
{
	prom_linked_list_node_t *node;
	prom_linked_list_node_t *prev_node;

	if (self == NULL)
		return -EINVAL;

	/* Locate the node */
	prev_node = NULL;
	for (node = self->head; node != NULL; node = node->next) {
		if (self->compare_fn) {
			if ((*self->compare_fn)(node->item, item)
			    == PROM_EQUAL) {
				break;
			}
		} else if (node->item == item) {
			break;
		}
		prev_node = node;
	}

	if (node == NULL)
		return 0;

	if (prev_node)
		prev_node->next = node->next;
	else
		self->head = node->next;

	if (node->next == NULL)
		self->tail = prev_node;

	if (node->item != NULL) {
		if (self->free_fn)
			(*self->free_fn)(node->item);
		else
			free(node->item);
	}

	node->item = NULL;
	free(node);
	node = NULL;
	self->size--;

	return 0;
}

prom_linked_list_compare_t
prom_linked_list_compare(prom_linked_list_t *self, void *item_a, void *item_b)
{
	if (self == NULL)
		return 1;
	if (self->compare_fn)
		return (*self->compare_fn)(item_a, item_b);

	return strcmp(item_a, item_b);
}

size_t
prom_linked_list_size(prom_linked_list_t *self)
{
	return self->size;
}

int
prom_linked_list_set_free_fn(prom_linked_list_t *self,
    prom_linked_list_free_item_fn free_fn)
{
	if (self == NULL)
		return -EINVAL;

	self->free_fn = free_fn;
	return 0;
}

int
prom_linked_list_set_compare_fn(prom_linked_list_t *self,
    prom_linked_list_compare_item_fn compare_fn)
{
	if (self == NULL)
		return -EINVAL;

	self->compare_fn = compare_fn;
	return 0;
}

void
prom_linked_list_no_op_free(void *item)
{
}
