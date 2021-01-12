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
#include "monitor/http/promhttp.h"

#include <string.h>

#include "monitor/def/prom.h"

prom_collector_registry_t *prom_active_registry;

void
promhttp_set_active_collector_registry(
    prom_collector_registry_t *active_registry)
{
	if (!active_registry)
		prom_active_registry = PROM_COLLECTOR_REGISTRY_DEFAULT;
	else
		prom_active_registry = active_registry;
}

int
promhttp_handler(void *cls, struct MHD_Connection *connection, const char *url,
    const char *method, const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response;
	char const *buf;
	int ret;

	if (strcmp(method, "GET") != 0) {
		buf = "Invalid HTTP Method\n";
		response = MHD_create_response_from_buffer(strlen(buf),
		    (void *)buf, MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST,
		    response);
		goto end;
	}
	if (strcmp(url, "/") == 0) {
		buf = "OK\n";
		response = MHD_create_response_from_buffer(strlen(buf),
		    (void *)buf, MHD_RESPMEM_PERSISTENT);
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		goto end;
	}
	if (strcmp(url, "/metrics") == 0) {
		buf = prom_collector_registry_bridge(prom_active_registry);
		response = MHD_create_response_from_buffer(strlen(buf),
		    (void *)buf, MHD_RESPMEM_MUST_FREE);
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		goto end;
	}
	buf = "Bad Request\n";
	response = MHD_create_response_from_buffer(strlen(buf), (void *)buf,
	    MHD_RESPMEM_PERSISTENT);
	ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
end:
	MHD_destroy_response(response);
	return ret;
}

struct MHD_Daemon *
promhttp_start_daemon(unsigned int flags, unsigned short port,
    MHD_AcceptPolicyCallback apc, void *apc_cls)
{
	return MHD_start_daemon(flags, port, apc, apc_cls, &promhttp_handler,
	    NULL, MHD_OPTION_END);
}
