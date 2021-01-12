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
#include "monitor/def/prom_metric_sample.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* Private */
#include "monitor/impl/prom_errors.h"
#include "monitor/impl/prom_log.h"
#include "monitor/impl/prom_metric_sample_i.h"
#include "monitor/impl/prom_metric_sample_t.h"

prom_metric_sample_t *
prom_metric_sample_new(prom_metric_type_t type, const char *l_value,
    double r_value)
{
	prom_metric_sample_t *self;

	self = malloc(sizeof(prom_metric_sample_t));
	if (self == NULL) {
		pr_enomem();
		return NULL;
	}

	self->l_value = strdup(l_value);
	if (self->l_value == NULL) {
		free(self);
		pr_enomem();
		return NULL;
	}

	self->type = type;
	self->r_value = ATOMIC_VAR_INIT(r_value);

	return self;
}

int
prom_metric_sample_destroy(prom_metric_sample_t *self)
{
	if (self == NULL)
		return 0;

	free((void *)self->l_value);
	self->l_value = NULL;
	free((void *)self);
	self = NULL;

	return 0;
}

int
prom_metric_sample_destroy_generic(void *gen)
{
	prom_metric_sample_t *self;
	int error;

	if (gen == NULL)
		return -EINVAL;

	self = (prom_metric_sample_t *)gen;
	error = prom_metric_sample_destroy(self);
	self = NULL;

	return error;
}

void
prom_metric_sample_free_generic(void *gen)
{
	prom_metric_sample_destroy((prom_metric_sample_t *)gen);
}

int
prom_metric_sample_add(prom_metric_sample_t *self, double r_value)
{
	_Atomic double old;
	_Atomic double new;

	if (self == NULL)
		return -EINVAL;

	if (r_value < 0)
		return -EINVAL;

	old = atomic_load(&self->r_value);
	for (;;) {
		new = ATOMIC_VAR_INIT(old + r_value);
		if (atomic_compare_exchange_weak(&self->r_value, &old, new))
			return 0;
	}

	/* "Unreachable" */
	return -EINVAL;
}

int
prom_metric_sample_sub(prom_metric_sample_t *self, double r_value)
{
	_Atomic double old;
	_Atomic double new;

	if (self == NULL)
		return -EINVAL;

	if (self->type != PROM_GAUGE)
		return pr_val_err(PROM_METRIC_INCORRECT_TYPE);

	old = atomic_load(&self->r_value);
	for (;;) {
		new = ATOMIC_VAR_INIT(old - r_value);
		if (atomic_compare_exchange_weak(&self->r_value, &old, new))
			return 0;
	}

	/* "Unreachable" */
	return -EINVAL;
}

int
prom_metric_sample_set(prom_metric_sample_t *self, double r_value)
{
	if (self->type != PROM_GAUGE)
		return pr_val_err(PROM_METRIC_INCORRECT_TYPE);

	atomic_store(&self->r_value, r_value);
	return 0;
}
