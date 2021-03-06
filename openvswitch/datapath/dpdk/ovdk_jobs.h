/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2014 NEC Laboratories Europe Ltd. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NEC Laboratories Europe Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _OVDK_JOBS_H_
#define _OVDK_JOBS_H_

#include <rte_lcore.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

#define MAXJOBS_PER_LCORE 32

typedef void (lcore_job_t)(void *);

struct ovdk_job {
	lcore_job_t *func;
	void *arg;
};

struct ovdk_joblist {
	volatile int online; /* if 0, loop exit is requested */

	unsigned nb_jobs;
	struct ovdk_job jobs[MAXJOBS_PER_LCORE];
} __rte_cache_aligned;

extern struct ovdk_joblist **ovdk_joblist_refs;

/*
 * Initialization
 */
void ovdk_jobs_init(void);

/*
 * Job management
 */
void ovdk_jobs_clear_lcore(unsigned lcore_id);
void ovdk_jobs_clear_all(void);
int ovdk_jobs_add_to_lcore(lcore_job_t *f, void *arg, unsigned lcore_id);

/*
 * Job execution
 */
int ovdk_jobs_launch_slave_lcore(unsigned lcore_id);
void ovdk_jobs_launch_slaves_all(void);
int ovdk_jobs_stop_slave_lcore(unsigned lcore_id);
void ovdk_jobs_stop_slaves_all(void);
#define OVDK_JOBS_LCORE_IS_ONLINE(lcore_id) \
	(rte_eal_get_lcore_state((lcore_id)) == RUNNING)
#define OVDK_JOBS_LCORE_HAS_JOBS(lcore_id) \
	(ovdk_joblist_refs[(lcore_id)]->nb_jobs > 0)

/**
 * Runs a single job iteration on master lcore
 * This function differs from the slave lcore loop because it
 * supports its reconfiguration even within a job execution.
 *
 * Note: Call this function only on master lcore.
 * Note: Looping logic has to be done by the caller.
 * Note: The online flag is not used on master lcore. If this lcore
 *       shall stop processing, stop calling ovdk_jobs_run_master_lcore().
 * Note: Job execution throughput is expected to be lower on
 *       master lcore. Please avoid running performance critical
 *       jobs this lcore.
 */
static inline void __attribute__((always_inline))
ovdk_jobs_run_master_lcore(void)
{
	struct ovdk_joblist *joblist = ovdk_joblist_refs[rte_lcore_id()];
	static unsigned i = ((unsigned) ((signed) -1));

	if (likely(joblist->nb_jobs > 0)) {
		i = (i + 1) % joblist->nb_jobs;
		joblist->jobs[i].func(joblist->jobs[i].arg);
	}
}

#endif /* _OVDK_JOBS_H_ */
