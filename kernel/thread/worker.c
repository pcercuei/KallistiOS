/* KallistiOS ##version##

   worker.c
   Copyright (C) 2024 Paul Cercueil
*/

#include <kos/genwait.h>
#include <kos/thread.h>
#include <kos/worker_thread.h>
#include <stdbool.h>
#include <stdlib.h>

struct kthread_worker {
    kthread_t *thd;
    void (*routine)(void *);
    void *data;
    bool pending;
    bool quit;
};

static void *thd_worker_thread(void *d) {
    kthread_worker_t *worker = d;
    uint32_t flags;

    for (;;) {
        flags = irq_disable();

        if (!worker->pending)
            genwait_wait(worker, worker->thd->label, 0, NULL);

        irq_restore(flags);

        if (worker->quit)
            break;

        worker->pending = false;
        worker->routine(worker->data);
    }

    return NULL;
}

kthread_worker_t *thd_worker_create(void (*routine)(void *), void *data) {
    kthread_worker_t *worker;
    uint32_t flags;

    worker = malloc(sizeof(*worker));
    if (!worker)
        return NULL;

    worker->data = data;
    worker->routine = routine;
    worker->pending = false;
    worker->quit = false;

    flags = irq_disable();

    worker->thd = thd_create(0, thd_worker_thread, worker);
    if (!worker->thd) {
        irq_restore(flags);
        free(worker);
        return NULL;
    }

    irq_restore(flags);

    return worker;
}

void thd_worker_wakeup(kthread_worker_t *worker) {
    uint32_t flags = irq_disable();

    worker->pending = true;
    genwait_wake_one(worker);

    irq_restore(flags);
}

void thd_worker_destroy(kthread_worker_t *worker) {
    worker->quit = true;
    genwait_wake_one(worker);

    thd_join(worker->thd, NULL);
    free(worker);
}

kthread_t *thd_worker_get_thread(kthread_worker_t *worker) {
    return worker->thd;
}
