/* KallistiOS ##version##

   include/kos/worker_thread.h
   Copyright (C) 2024 Paul Cercueil
*/

/** \file    kos/worker_thread.h
    \brief   Threaded worker support.
    \ingroup kthreads

    This file contains the threaded worker API. Threaded workers are threads
    that are idle most of the time, until they are notified that there is work
    pending; in which case they will call their associated work function.

    \author Paul Cercueil

    \see    kos/thread.h
*/

#ifndef __KOS_WORKER_THREAD_H
#define __KOS_WORKER_THREAD_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/thread.h>

struct kthread_worker;

/** \brief   Structure describing one worker thread.

    \headerfile kos/thread.h
*/
typedef struct kthread_worker kthread_worker_t;

/** \brief       Create a new worker thread.
    \relatesalso kthread_worker_t

    This function will create a thread that will call the given routine with the
    given param pointer when notified.
    The thread will only stop when thd_worker_destroy() is called.

    \param  routine         The function to call in the worker thread.
    \param  data            A parameter to pass to the function called.

    \return                 The new worker thread on success, NULL on failure.

    \sa thd_worker_destroy, thd_worker_wakeup
*/
kthread_worker_t *thd_worker_create(void (*routine)(void *), void *data);

/** \brief       Stop and destroy a worker thread.
    \relatesalso kthread_worker_t

    This function will stop the worker thread and free its memory.

    \param  thd             The worker thread to destroy.

    \sa thd_worker_create, thd_worker_wakeup
*/
void thd_worker_destroy(kthread_worker_t *thd);

/** \brief       Wake up a worker thread.
    \relatesalso kthread_worker_t

    This function will wake up the worker thread, causing it to call its
    corresponding work function.

    \param  thd             The worker thread to wake up.

    \sa thd_worker_create, thd_worker_destroy
*/
void thd_worker_wakeup(kthread_worker_t *thd);

/** \brief       Get a handle to the underlying thread.
    \relatesalso kthread_worker_t

    \param  thd             The worker thread whose handle should be returned.

    \return                 A handle to the underlying thread.
*/
kthread_t *thd_worker_get_thread(kthread_worker_t *thd);

__END_DECLS

#endif /* __KOS_WORKER_THREAD_H */
