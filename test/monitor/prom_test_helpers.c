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

#include <stdio.h>
#include <stdlib.h>

#include "monitor/http/promhttp.c"
#include "monitor/prom_impersonator.h"

#include "common.c"
#include "file.c"
#include "impersonator.c"
#include "line_file.c"
#include "log.c"
#include "thread/thread_pool.c"

#define PROMTEST_THREAD_POOL_SIZE 5

typedef void (*setup_cb)(void);

struct MHD_Daemon *promtest_daemon;

int
promtest_setup(setup_cb cb)
{
	/* Initialize the default collector registry */
	prom_collector_registry_default_init();

	/* Set the counter, gauge, etc. */
	cb();

	/* Set the collector registry on the handler to the default registry */
	promhttp_set_active_collector_registry(NULL);

	/* Start the HTTP server */
	promtest_daemon = promhttp_start_daemon(MHD_USE_SELECT_INTERNALLY, 8000,
	    NULL, NULL);

	if (promtest_daemon == NULL) {
		ck_abort_msg("promtest_daemon == NULL");
		return EINVAL;
	}

	return 0;
}

void
promtest_teardown(void)
{
	/*
	 * Destroy the default registry. This effectively deallocates all
	 * metrics registered to it, including itself.
	 */
	prom_collector_registry_destroy(PROM_COLLECTOR_REGISTRY_DEFAULT);
	PROM_COLLECTOR_REGISTRY_DEFAULT = NULL;

	/* Stop the HTTP server */
	MHD_stop_daemon(promtest_daemon);
	promtest_daemon = NULL;
}

/*
 * @brief Parse the output and set the value of the foo_counter metric.
 *
 * We must past a pointer to a char* so the value gets updated
 */
static int
promtest_parse_output(char const *metric_id, struct line_file *lf, char **value)
{
	size_t id_len;
	char *ptr;
	char *tmp;
	int error;

	tmp = NULL;
	ptr = NULL;
	id_len = strlen(metric_id);
	while ((error = lfile_read(lf, &ptr)) == 0) {
		/* EOF */
		if (ptr == NULL)
			break;

		if (strncmp(ptr, metric_id, id_len) == 0) {
			/* Ignore the space after the ID */
			ptr = ptr + id_len + 1;

			tmp = strdup(ptr);
			if (tmp == NULL)
				return ENOMEM;
			*value = tmp;
			return 0;
		}
	}

	if (error)
		return error;

	/* No error, but the string wasn't found */
	*value = NULL;
	return ENOENT;
}

int
promtest_fetch_metric(thread_pool_task_cb cb, char const *metric_id,
    char **value)
{
	struct thread_pool *thread_pool;
	struct line_file lf;
	FILE *f;
	char *tmp;
	int i;
	int error;

	thread_pool = NULL;
	error = thread_pool_create(PROMTEST_THREAD_POOL_SIZE, &thread_pool);
	if (error)
		goto end;

	/* Assign work to each thread */
	for (i = 0; i < PROMTEST_THREAD_POOL_SIZE; i++) {
		error = thread_pool_push(thread_pool, cb, NULL);
		if (error)
			goto end_threads;
	}

	/* Wait for all */
	thread_pool_wait(thread_pool);

	/* Scrape the endpoint */
	f = popen("curl http://0.0.0.0:8000/metrics", "r");
	if (f == NULL) {
		/* Shell out failed */
		ck_abort_msg("Shell out failed");
		error = EINVAL;
		goto end_threads;
	}

	lf.file = f;
	lf.file_name = NULL;
	lf.offset = 0;

	/* Parse the output */
	tmp = NULL;
	error = promtest_parse_output(metric_id, &lf, &tmp);
	if (error) {
		/* Failed to parse output */
		ck_abort_msg("Failed to parse output");
		goto end_pipe;
	}

	*value = tmp;

	/* OK */
	error = 0;
end_pipe:
	pclose(f);
end_threads:
	thread_pool_destroy(thread_pool);
end:
	promtest_teardown();
	return error;
}

