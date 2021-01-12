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

#ifndef SRC_MONITOR_IMPL_PROM_PROCESS_STAT_T_H
#define SRC_MONITOR_IMPL_PROM_PROCESS_STAT_T_H

#include "monitor/def/prom_gauge.h"
#include "monitor/impl/prom_procfs_t.h"

extern prom_gauge_t *prom_process_cpu_seconds_total;
extern prom_gauge_t *prom_process_virtual_memory_bytes;
extern prom_gauge_t *prom_process_start_time_seconds;

/*
 * @brief Refer to man proc and search for /proc/[pid]/stat
 */
typedef struct prom_process_stat {
	/* (1) pid  %d */
	int pid;
	/* (2) comm  %s */
	char *comm;
	/* (3) state  %c */
	char state;
	/* (4) ppid  %d */
	int ppid;
	/* (5) pgrp  %d */
	int pgrp;
	/* (6) session  %d */
	int session;
	/* (7) tty_nr  %d */
	int tty_nr;
	/* (8) tpgid  %d */
	int tpgid;
	/* (9) flags  %u */
	unsigned flags;
	/* (10) minflt  %lu */
	unsigned long minflt;
	/* (11) cminflt  %lu */
	unsigned long cminflt;
	/* (12) majflt  %lu */
	unsigned long majflt;
	/* (13) cmajflt  %lu */
	unsigned long cmajflt;
	/* (14) utime  %lu */
	unsigned long utime;
	/* (15) stime  %lu */
	unsigned long stime;
	/* (16) cutime  %ld */
	long int cutime;
	/* (17) cstime  %ld */
	long int cstime;
	/* (18) priority  %ld */
	long int priority;
	/* (19) nice  %ld */
	long int nice;
	/* (20) num_threads  %ld */
	long int num_threads;
	/* (21) itrealvalue  %ld */
	long int itrealvalue;
	/* (22) starttime  %llu */
	unsigned long long starttime;
	/* (23) vsize  %lu */
	unsigned long vsize;
	/* (24) rss  %ld */
	long int rss;
	/* (25) rsslim  %lu */
	unsigned long rsslim;
	/* (26) startcode  %lu  [PT] */
	unsigned long startcode;
	/* (27) endcode  %lu  [PT] */
	unsigned long endcode;
	/* (28) startstack  %lu  [PT] */
	unsigned long startstack;
	/* (29) kstkesp  %lu  [PT] */
	unsigned long kstkesp;
	/* (30) kstkeip  %lu  [PT] */
	unsigned long kstkeip;
	/* (31) signal  %lu */
	unsigned long signal;
	/* (32) blocked  %lu */
	unsigned long blocked;
	/* (33) sigignore  %lu */
	unsigned long sigignore;
	/* (34) sigcatch  %lu */
	unsigned long sigcatch;
	/* (35) wchan  %lu  [PT] */
	unsigned long wchan;
	/* (36) nswap  %lu */
	unsigned long nswap;
	/* (37) cnswap  %lu */
	unsigned long cnswap;
	/* (38) exit_signal  %d  (since Linux 2.1.22) */
	int exit_signal;
	/* (39) processor  %d  (since Linux 2.2.8) */
	int processor;
	/* (40) rt_priority  %u  (since Linux 2.5.19) */
	unsigned rt_priority;
	/* (41) policy  %u  (since Linux 2.5.19) */
	unsigned policy;
	/* (42) delayacct_blkio_ticks */
	unsigned long long delayacct_blkio_ticks;
	/* (43) guest_time  %lu  (since Linux 2.6.24) */
	unsigned long guest_time;
	/* (44) cguest_time  %ld  (since Linux 2.6.24) */
	long int cguest_time;
	/* (45) start_data  %lu  (since Linux 3.3)  [PT] */
	unsigned long start_data;
	/* (46) end_data  %lu  (since Linux 3.3)  [PT] */
	unsigned long end_data;
	/* (47) start_brk  %lu  (since Linux 3.3)  [PT] */
	unsigned long start_brk;
	/* (48) arg_start  %lu  (since Linux 3.5)  [PT] */
	unsigned long arg_start;
	/* (49) arg_end  %lu  (since Linux 3.5)  [PT] */
	unsigned long arg_end;
	/* (50) env_start  %lu  (since Linux 3.5)  [PT] */
	unsigned long env_start;
	/* (51) env_end  %lu  (since Linux 3.5)  [PT] */
	unsigned long env_end;
	/* (52) exit_code  %d  (since Linux 3.5)  [PT] */
	int exit_code;
} prom_process_stat_t;

typedef prom_procfs_buf_t prom_process_stat_file_t;

#endif /* SRC_MONITOR_IMPL_PROM_PROCESS_STAT_T_H */
