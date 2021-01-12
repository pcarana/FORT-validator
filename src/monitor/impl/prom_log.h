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

#ifndef SRC_MONITOR_IMPL_PROM_LOG_H
#define SRC_MONITOR_IMPL_PROM_LOG_H

#include <stdio.h>
#include "log.h"

#define PROM_LOG(msg) pr_op_debug("%s %d %s", __FILE__, __LINE__, msg);

#endif /* SRC_MONITOR_IMPL_PROM_LOG_H */
