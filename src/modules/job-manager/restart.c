/************************************************************\
 * Copyright 2018 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* restart - reload active jobs from the KVS */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <flux/core.h>

#include "src/common/libutil/fluid.h"
#include "src/common/libutil/errprintf.h"
#include "src/common/libczmqcontainers/czmq_containers.h"

#include "job.h"
#include "restart.h"
#include "event.h"
#include "wait.h"
#include "jobtap-internal.h"

/* restart_map callback should return -1 on error to stop map with error,
 * or 0 on success.  'job' is only valid for the duration of the callback.
 */
typedef int (*restart_map_f)(struct job *job, void *arg, flux_error_t *error);

const char *checkpoint_key = "checkpoint.job-manager";

int restart_count_char (const char *s, char c)
{
    int count = 0;
    while (*s) {
        if (*s++ == c)
            count++;
    }
    return count;
}

static struct job *lookup_job (flux_t *h, flux_jobid_t id, flux_error_t *error)
{
    flux_future_t *f1 = NULL;
    flux_future_t *f2 = NULL;
    char k1[64], k2[64];
    const char *eventlog, *jobspec;
    struct job *job = NULL;
    flux_error_t e;

    if (flux_job_kvs_key (k1, sizeof (k1), id, "eventlog") < 0
        || flux_job_kvs_key (k2, sizeof (k2), id, "jobspec") < 0
        || !(f1 = flux_kvs_lookup (h, NULL, 0, k1))
        || !(f2 = flux_kvs_lookup (h, NULL, 0, k2))) {
        errprintf (error,
                   "cannot send lookup requests for job %ju: %s",
                   (uintmax_t)id,
                   strerror (errno));
        goto done;
    }
    if (flux_kvs_lookup_get (f1, &eventlog) < 0) {
        errprintf (error, "lookup %s: %s", k1, strerror (errno));
        goto done;
    }
    if (flux_kvs_lookup_get (f2, &jobspec) < 0) {
        errprintf (error, "lookup %s: %s", k2, strerror (errno));
        goto done;
    }
    if (!(job = job_create_from_eventlog (id, eventlog, jobspec, &e)))
        errprintf (error, "replay %s: %s", k1, e.text);
done:
    flux_future_destroy (f1);
    flux_future_destroy (f2);
    return job;
}

static int depthfirst_map_one (flux_t *h,
                               const char *key,
                               int dirskip,
                               restart_map_f cb,
                               void *arg,
                               flux_error_t *error)
{
    flux_jobid_t id;
    struct job *job = NULL;
    int rc = -1;

    if (strlen (key) <= dirskip) {
        errprintf (error, "internal error key=%s dirskip=%d", key, dirskip);
        errno = EINVAL;
        return -1;
    }
    if (fluid_decode (key + dirskip + 1, &id, FLUID_STRING_DOTHEX) < 0) {
        errprintf (error, "could not decode %s to job ID", key + dirskip + 1);
        return -1;
    }
    if (!(job = lookup_job (h, id, error)))
        return -1;
    if (cb (job, arg, error) < 0)
        goto done;
    rc = 1;
done:
    job_decref (job);
    return rc;
}

static int depthfirst_map (flux_t *h,
                           const char *key,
                           int dirskip,
                           restart_map_f cb,
                           void *arg,
                           flux_error_t *error)
{
    flux_future_t *f;
    const flux_kvsdir_t *dir;
    flux_kvsitr_t *itr;
    const char *name;
    int path_level;
    int count = 0;
    int rc = -1;

    path_level = restart_count_char (key + dirskip, '.');
    if (!(f = flux_kvs_lookup (h, NULL, FLUX_KVS_READDIR, key))) {
        errprintf (error,
                   "cannot send lookup request for %s: %s",
                   key,
                   strerror (errno));
        return -1;
    }
    if (flux_kvs_lookup_get_dir (f, &dir) < 0) {
        if (errno == ENOENT && path_level == 0)
            rc = 0;
        else {
            errprintf (error,
                       "could not look up %s: %s",
                       key,
                       strerror (errno));
        }
        goto done;
    }
    if (!(itr = flux_kvsitr_create (dir))) {
        errprintf (error,
                   "could not create iterator for %s: %s",
                   key,
                   strerror (errno));
        goto done;
    }
    while ((name = flux_kvsitr_next (itr))) {
        char *nkey;
        int n;
        if (!flux_kvsdir_isdir (dir, name))
            continue;
        if (!(nkey = flux_kvsdir_key_at (dir, name))) {
            errprintf (error,
                       "could not build key for %s in %s: %s",
                       name,
                       key,
                       strerror (errno));
            goto done_destroyitr;
        }
        if (path_level == 3) // orig 'key' = .A.B.C, thus 'nkey' is complete
            n = depthfirst_map_one (h, nkey, dirskip, cb, arg, error);
        else
            n = depthfirst_map (h, nkey, dirskip, cb, arg, error);
        if (n < 0) {
            int saved_errno = errno;
            free (nkey);
            errno = saved_errno;
            goto done_destroyitr;
        }
        count += n;
        free (nkey);
    }
    rc = count;
done_destroyitr:
    flux_kvsitr_destroy (itr);
done:
    flux_future_destroy (f);
    return rc;
}

/* reload_map_f callback
 * The job state/flags has been recreated by replaying the job's eventlog.
 * Enqueue the job and kick off actions appropriate for job's current state.
 */
static int restart_map_cb (struct job *job, void *arg, flux_error_t *error)
{
    struct job_manager *ctx = arg;

    if (zhashx_insert (ctx->active_jobs, &job->id, job) < 0) {
        errprintf (error,
                   "could not insert job %ju into active job hash",
                   (uintmax_t)job->id);
        return -1;
    }
    if (ctx->max_jobid < job->id)
        ctx->max_jobid = job->id;
    if ((job->flags & FLUX_JOB_WAITABLE))
        wait_notify_active (ctx->wait, job);
    if (event_job_action (ctx->event, job) < 0) {
        flux_log_error (ctx->h,
                        "replay warning: %s action failed on job %ju",
                        flux_job_statetostr (job->state, "L"),
                        (uintmax_t)job->id);
    }
    return 0;
}

int restart_save_state_to_txn (struct job_manager *ctx, flux_kvs_txn_t *txn)
{
    if (flux_kvs_txn_pack (txn,
                           0,
                           checkpoint_key,
                           "{s:I}",
                           "max_jobid",
                           ctx->max_jobid) < 0)
        return -1;
    return 0;
}

int restart_save_state (struct job_manager *ctx)
{
    flux_future_t *f = NULL;
    flux_kvs_txn_t *txn;
    int rc = -1;

    if (!(txn = flux_kvs_txn_create ())
        || restart_save_state_to_txn (ctx, txn) < 0
        || !(f = flux_kvs_commit (ctx->h, NULL, 0, txn))
        || flux_future_get (f, NULL) < 0)
        goto done;
    rc = 0;
done:
    flux_future_destroy (f);
    flux_kvs_txn_destroy (txn);
    return rc;
}

static int restart_restore_state (struct job_manager *ctx)
{
    flux_future_t *f;
    flux_jobid_t id;

    if (!(f = flux_kvs_lookup (ctx->h, NULL, 0, checkpoint_key)))
        return -1;
    if (flux_kvs_lookup_get_unpack (f,
                                    "{s:I}",
                                    "max_jobid", &id) < 0) {
        flux_future_destroy (f);
        return -1;
    }
    if (ctx->max_jobid < id)
        ctx->max_jobid = id;
    flux_future_destroy (f);
    return 0;
}

int restart_from_kvs (struct job_manager *ctx)
{
    const char *dirname = "job";
    int dirskip = strlen (dirname);
    int count;
    struct job *job;
    flux_error_t error;

    /* Load any active jobs present in the KVS at startup.
     */
    count = depthfirst_map (ctx->h,
                            dirname,
                            dirskip,
                            restart_map_cb,
                            ctx,
                            &error);
    if (count < 0) {
        flux_log (ctx->h, LOG_ERR, "restart failed: %s", error.text);
        return -1;
    }
    flux_log (ctx->h, LOG_INFO, "restart: %d jobs", count);
    /* Post flux-restart to any jobs in SCHED state, so they may
     * transition back to PRIORITY and re-obtain the priority.
     *
     * Initialize the count of "running" jobs
     */
    job = zhashx_first (ctx->active_jobs);
    while (job) {
        if (job->state == FLUX_JOB_STATE_NEW
            || job->state == FLUX_JOB_STATE_DEPEND) {
            char *errmsg = NULL;
            if (jobtap_check_dependencies (ctx->jobtap,
                                           job,
                                           true,
                                           &errmsg) < 0) {
                flux_log (ctx->h, LOG_ERR,
                          "restart: id=%ju: dependency check failed: %s",
                          (uintmax_t) job->id, errmsg);
            }
            free (errmsg);
        }
        /*
         *  On restart, call 'job.create' and 'job.new' plugin callbacks
         *   since this is the first time this instance of the job-manager
         *   has seen this job. Be sure to call these before posting any
         *   other events below, since these should always be the first
         *   callbacks for a job.
         *
         *  Jobs in SCHED state may also immediately transition back to
         *   PRIORITY, potentially generating two other plugin callbacks
         *   after this one. (job.priority, job.sched...)
         */
        if (jobtap_call (ctx->jobtap, job, "job.create", NULL) < 0)
            flux_log_error (ctx->h, "jobtap_call (id=%ju, create)",
                                (uintmax_t) job->id);
        if (jobtap_call (ctx->jobtap, job, "job.new", NULL) < 0)
            flux_log_error (ctx->h, "jobtap_call (id=%ju, new)",
                                (uintmax_t) job->id);

        if (job->state == FLUX_JOB_STATE_SCHED) {
            /*
             * This is confusing. In order to update priority on transition
             *  back to PRIORITY state, the priority must be reset to "-1",
             *  even though the last priority value was reconstructed from
             *  the eventlog. This is because the transitioning "priority"
             *  event is only posted when the priority changes.
             */
            job->priority = -1;
            if (event_job_post_pack (ctx->event,
                                     job,
                                     "flux-restart",
                                     0,
                                     NULL) < 0) {
                flux_log_error (ctx->h, "%s: event_job_post_pack id=%ju",
                                __FUNCTION__, (uintmax_t)job->id);
            }
        }
        else if ((job->state & FLUX_JOB_STATE_RUNNING) != 0) {
            ctx->running_jobs++;
            job->reattach = 1;
            if ((job->flags & FLUX_JOB_DEBUG)) {
                if (event_job_post_pack (ctx->event,
                                         job,
                                         "debug.exec-reattach-start",
                                         0,
                                         "{s:I}",
                                         "id", (uintmax_t)job->id) < 0)
                    flux_log_error (ctx->h, "%s: event_job_post_pack id=%ju",
                                    __FUNCTION__, (uintmax_t)job->id);
            }
        }
        job = zhashx_next (ctx->active_jobs);
    }
    flux_log (ctx->h, LOG_INFO, "restart: %d running jobs", ctx->running_jobs);

    /* Restore misc state.
     */
    if (restart_restore_state (ctx) < 0) {
        if (errno != ENOENT) {
            flux_log_error (ctx->h, "restart: %s", checkpoint_key);
            return -1;
        }
        flux_log (ctx->h, LOG_INFO, "restart: %s not found", checkpoint_key);
    }
    flux_log (ctx->h,
              LOG_DEBUG,
              "restart: max_jobid=%ju",
              (uintmax_t)ctx->max_jobid);
    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
