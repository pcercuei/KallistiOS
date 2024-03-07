#ifndef _AICA_LOCK_H
#define _AICA_LOCK_H

struct task;

struct mutex {
    struct task *owner;
};

#define MUTEX_INITIALIZER { 0 }

void mutex_lock(struct mutex *lock);
void mutex_unlock(struct mutex *lock);
_Bool mutex_trylock(struct mutex *lock);

#endif /* _AICA_LOCK_H */
