/* KallistiOS ##version##

   task.c
   Copyright (C) 2024 Paul Cercueil

   Mutex implementation
*/

#include <aicaos/irq.h>
#include <aicaos/lock.h>
#include <aicaos/task.h>
#include <stddef.h>

void mutex_lock(struct mutex *lock)
{
    irq_ctx_t cxt = irq_disable();

    while (lock->owner) {
        task_boost(lock->owner);
        task_wait(lock);
    }

    lock->owner = current_task;

    irq_restore(cxt);
}

_Bool mutex_trylock(struct mutex *lock)
{
    irq_ctx_t cxt;
    _Bool success;

    cxt = irq_disable();

    success = !lock->owner;
    if (!lock->owner)
        lock->owner = current_task;

    irq_restore(cxt);

    return success;
}

void mutex_unlock(struct mutex *lock)
{
    irq_ctx_t cxt = irq_disable();

    lock->owner = NULL;
    task_wake(lock, 1);
    task_unboost();

    irq_restore(cxt);
}
