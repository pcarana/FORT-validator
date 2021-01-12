/**
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
#include <check.h>
#include <stdlib.h>

#include "monitor/prom_test_helpers.c"

#define TEST_COUNTER "foo_counter"
#define TEST_GAUGE   "foo_gauge"

prom_counter_t *foo_counter;
prom_gauge_t *foo_gauge;

static void
init_counter(void)
{
	foo_counter = prom_collector_registry_must_register_metric(
	    prom_counter_new(TEST_COUNTER, "counter for foo", 0, NULL));
}

static void
init_gauge(void)
{
	foo_gauge = prom_collector_registry_must_register_metric(
	    prom_gauge_new(TEST_GAUGE, "gauge for foo", 0, NULL));
}

/*
 * @brief The entrypoint to a worker thread within the counter_test
 */
static void *
promtest_counter_handler(void *arg)
{
	int i;

	for (i = 0; i < 1000000; i++)
		prom_counter_inc(foo_counter, NULL);

	return NULL;
}

/*
 * @brief The entrypoint to a worker thread within the gauge_test
 */
static void *
promtest_gauge_handler(void *arg)
{
	int i;

	for (i = 0; i < 1000000; i++)
		prom_gauge_inc(foo_gauge, NULL);

	return NULL;
}

/*
 * @brief For each thread in a threadpool of 5 we increment a single counter 1
 * million times
 *
 * The purpose of this test is to check for deadlock and race conditions
 */
static int
promtest_counter(char **value)
{
	int error;

	if (promtest_setup(&init_counter)) {
		/* Failed to setup promtest_counter */
		ck_abort_msg("Failed to setup promtest_counter");
		return EINVAL;
	}

	error = promtest_fetch_metric(promtest_counter_handler, TEST_COUNTER,
	    value);
	promtest_teardown();

	return error;
}

/*
 * @brief For each thread in a threadpool of 5 we increment a single gauge 1
 * million times
 *
 * The purpose of this test is to check for deadlock and race conditions
 */
static int
promtest_gauge(char **value)
{
	int error;

	if (promtest_setup(&init_gauge)) {
		/* Failed to setup promtest_gauge */
		ck_abort_msg("Failed to setup promtest_gauge");
		return EINVAL;
	}

	error = promtest_fetch_metric(promtest_gauge_handler, TEST_GAUGE, value);
	promtest_teardown();

	return error;
}

START_TEST(counter_test)
{
	char *value;
	int error;

	value = NULL;
	error = promtest_counter(&value);

	ck_assert_int_eq(0, error);
	ck_assert_int_eq(0, strcmp("5000000.000000", value));
	free(value);
}
END_TEST

START_TEST(gauge_test)
{
	char *value;
	int error;

	value = NULL;
	error = promtest_gauge(&value);

	ck_assert_int_eq(0, error);
	ck_assert_int_eq(0, strcmp("5000000.000000", value));
	free(value);
}
END_TEST

Suite *prom_load_suite(void)
{
	Suite *suite;
	TCase *counter, *gauge;

	counter = tcase_create("Counter");
	tcase_set_timeout(counter, 30);
	tcase_add_test(counter, counter_test);

	gauge = tcase_create("Gauge");
	tcase_set_timeout(gauge, 30);
	tcase_add_test(gauge, gauge_test);

	suite = suite_create("Prom suite");
	suite_add_tcase(suite, counter);
	suite_add_tcase(suite, gauge);

	return suite;
}

int main(void)
{
	Suite *suite;
	SRunner *runner;
	int tests_failed;

	suite = prom_load_suite();

	runner = srunner_create(suite);
	srunner_run_all(runner, CK_NORMAL);
	tests_failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
