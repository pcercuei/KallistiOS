/* KallistiOS ##version##

   workqueue.c
   Copyright (C) 2025 Paul Cercueil
*/

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <kos/genwait.h>
#include <kos/mutex.h>
#include <kos/thread.h>
#include <kos/workqueue.h>

#include <arch/timer.h>

typedef struct workqueue {
    STAILQ_HEAD(workqueue_jobs, workqueue_job) jobs;
    kthread_t *thd;
    mutex_t lock;
    bool quit;
} workqueue_t;

static void *workqueue_thread(void *d) {
    workqueue_t *wq = d;
    workqueue_job_t *job;
    uint64_t now;
    int ret;

    while(!wq->quit) {
        mutex_lock(&wq->lock);

        job = STAILQ_FIRST(&wq->jobs);
        if(job) {
            STAILQ_REMOVE_HEAD(&wq->jobs, entry);
            now = timer_ms_gettime64();
        }

        mutex_unlock(&wq->lock);

        if(!job || job->time_ms >= now) {
            ret = genwait_wait(wq, wq->thd->label,
                               job ? job->time_ms - now : 0, NULL);
            if (!ret) {
                /* We did not time out, so something was added to the queue. */
                continue;
            }
        }

        job->cb(job);
    }

    return NULL;
}

workqueue_t *workqueue_create(void) {
    workqueue_t *wq;

    wq = calloc(1, sizeof(workqueue_t));
    if(!wq)
        return NULL;

    wq->lock = (mutex_t)MUTEX_INITIALIZER;

    mutex_lock(&wq->lock);

    wq->thd = thd_create(false, workqueue_thread, wq);
    if(!wq->thd) {
        mutex_unlock(&wq->lock);
        free(wq);
        return NULL;
    }

    thd_set_label(wq->thd, "workqueue");

    mutex_unlock(&wq->lock);

    return wq;
}

void workqueue_enqueue(workqueue_t *wq, workqueue_job_t *job) {
    workqueue_job_t *elm;

    mutex_lock_scoped(&wq->lock);

    if(!job->time_ms)
        job->time_ms = timer_ms_gettime64();

    STAILQ_FOREACH(elm, &wq->jobs, entry) {
        if(job->time_ms < elm->time_ms) {
            STAILQ_INSERT_AFTER(&wq->jobs, elm, job, entry);
            break;
        }
    }

    if(!elm)
        STAILQ_INSERT_TAIL(&wq->jobs, job, entry);
    genwait_wake_one(wq);
}

void workqueue_cancel(workqueue_t *wq, workqueue_job_t *job) {
    mutex_lock_scoped(&wq->lock);

    STAILQ_REMOVE(&wq->jobs, job, workqueue_job, entry);
    genwait_wake_one(wq);
}

void workqueue_kill(workqueue_t *wq) {
    if(!wq->quit) {
        wq->quit = true;
        genwait_wake_one(wq);
        thd_join(wq->thd, NULL);
    }
}

void workqueue_destroy(workqueue_t *wq) {
    workqueue_kill(wq);
    free(wq);
}
