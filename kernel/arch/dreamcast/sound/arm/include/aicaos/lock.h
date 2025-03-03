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

static inline void __mutex_scoped_cleanup(struct mutex **m) {
    mutex_unlock(*m);
}

#define ___mutex_lock_scoped(m, l) \
    struct mutex *__scoped_mutex_##l __attribute__((cleanup(__mutex_scoped_cleanup))) = (mutex_lock(m), (m))

#define __mutex_lock_scoped(m, l) ___mutex_lock_scoped(m, l)
#define mutex_lock_scoped(m) __mutex_lock_scoped((m), __LINE__)

#endif /* _AICA_LOCK_H */
