/* KallistiOS ##version##

   kernel/thread/thread.c
   Copyright (C) 2000, 2001, 2002, 2003 Megan Potter
   Copyright (C) 2010, 2016 Lawrence Sebald
   Copyright (C) 2023 Colton Pawielski
   Copyright (C) 2023, 2024 Falco Girgis
*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <reent.h>
#include <errno.h>
#include <kos/thread.h>
#include <kos/dbgio.h>
#include <kos/sem.h>
#include <kos/rwsem.h>
#include <kos/cond.h>
#include <kos/genwait.h>
#include <arch/irq.h>
#include <arch/timer.h>
#include <dc/perfctr.h>
#include <arch/arch.h>

/*

This module supports thread scheduling in KOS. The timer interrupt is used
to re-schedule the processor HZ times per second.
This is a fairly simplistic scheduler, though it does employ some
standard advanced OS tactics like priority scheduling and semaphores.

Some of this code ought to look familiar to BSD-heads; I studied the
BSD kernel quite a bit to get some ideas on priorities, and I am
also using their queue library verbatim (sys/queue.h).

*/

/* TLS Section ELF data - exported from linker script. */
extern int _tdata_start, _tdata_size;
extern int _tbss_size;
extern long _tdata_align, _tbss_align;

/* Utility function for aligning an address or offset. */
static inline size_t align_to(size_t address, size_t alignment) {
    return (address + (alignment - 1)) & ~(alignment - 1);
}

/*****************************************************************************/
/* Thread scheduler data */

/* Scheduler timer interrupt frequency (Hertz) */
static unsigned int thd_sched_ms = 1000 / HZ;

/* Thread list. This includes all threads except dead ones. */
static struct ktlist thd_list;

/* Run queue. This is more like on a standard time sharing system than the
   previous versions. The top element of this priority queue should be the
   thread that is ready to run next. When a thread is scheduled, it will be
   removed from this queue. When it's de-scheduled, it will be re-inserted
   by its priority value at the end of its priority group. Note that right
   now this condition is being broken because sleeping threads are on the
   same queue. We deal with those in thd_switch below. */
static struct ktqueue run_queue;

/* The currently executing thread. This thread should not be on any queues. */
kthread_t *thd_current = NULL;

/* Thread mode: uninitialized or pre-emptive. */
static kthread_mode_t thd_mode = THD_MODE_NONE;

/* Reaper semaphore. Counts the number of threads waiting to be reaped. */
static semaphore_t thd_reap_sem;

/* Number of threads active in the system. */
static size_t thd_count = 0;

/* The idle task */
static kthread_t *thd_idle_thd = NULL;

/*****************************************************************************/
/* Debug */

static const char *thd_state_to_str(kthread_t *thd) {
    switch(thd->state) {
        case STATE_ZOMBIE:
            return "zombie";
        case STATE_RUNNING:
            return "running";
        case STATE_READY:
            return "ready";
        case STATE_WAIT:

            if(thd->wait_msg)
                return thd->wait_msg;
            else
                return "wait";

        case STATE_FINISHED:
            return "finished";
        default:
            return "unknown";
    }
}

int thd_each(int (*cb)(kthread_t *thd, void *user_data), void *data) {
    kthread_t *cur;
    int retval;

    LIST_FOREACH(cur, &thd_list, t_list) {
        if((retval = cb(cur, data)))
            return retval;
    }

    return 0;
}

int thd_pslist(int (*pf)(const char *fmt, ...)) {
    uint64_t cpu_time, ns_time;
    kthread_t *cur;

    pf("All threads (may not be deterministic):\n");
    pf("addr\t  tid\tprio\tflags\t  wait_timeout\tcpu_time\t      state\t  name\n");

    LIST_FOREACH(cur, &thd_list, t_list) {
        pf("%08lx  ", CONTEXT_PC(cur->context));
        pf("%d\t", cur->tid);

        if(cur->prio == PRIO_MAX)
            pf("MAX\t");
        else
            pf("%d\t", cur->prio);

        pf("%08lx  ", cur->flags);
        pf("%12lu", (uint32_t)cur->wait_timeout);

        ns_time = perf_cntr_timer_ns();
        cpu_time = thd_get_cpu_time(cur);

        pf("%12llu (%6.3lf%%)  ",
            cpu_time, (double)cpu_time / (double)ns_time * 100.0);

        pf("%-10s  ", thd_state_to_str(cur));
        pf("%-10s\n", cur->label);
    }
    pf("--end of list--\n");

    return 0;
}

int thd_pslist_queue(int (*pf)(const char *fmt, ...)) {
    kthread_t *cur;

    pf("Queued threads:\n");
    pf("addr\t\ttid\tprio\tflags\twait_timeout\tstate     name\n");
    TAILQ_FOREACH(cur, &run_queue, thdq) {
        pf("%08lx\t", CONTEXT_PC(cur->context));
        pf("%d\t", cur->tid);

        if(cur->prio == PRIO_MAX)
            pf("MAX\t");
        else
            pf("%d\t", cur->prio);

        pf("%08lx\t", cur->flags);
        pf("%ld\t\t", (uint32_t)cur->wait_timeout);
        pf("%10s", thd_state_to_str(cur));
        pf("%s\n", cur->label);
    }

    return 0;
}

/*****************************************************************************/
/* Returns a fresh thread ID for each new thread */

/* Highest thread id (used when assigning next thread id) */
static tid_t tid_highest;

/* Return the next available thread id (assumes wraparound will not run
   into old processes). */
static tid_t thd_next_free(void) {
    int id;
    id = tid_highest++;
    return id;
}

/* Given a thread ID, locates the thread structure */
kthread_t *thd_by_tid(tid_t tid) {
    kthread_t *np;

    LIST_FOREACH(np, &thd_list, t_list) {
        if(np->tid == tid)
            return np;
    }

    return NULL;
}


/*****************************************************************************/
/* Thread support routines: idle task and start task wrapper */

/* An idle function. This function literally does nothing but loop
   forever. It's meant to be used for an idle task. */
static void *thd_idle_task(void *param) {
    /* Uncomment these if you want some debug for deadlocking */
    /*  int old = irq_disable();
    #ifndef NDEBUG
        thd_pslist();
        printf("Inside idle task now\n");
    #endif
        irq_restore(old); */
    (void)param;

    for(;;) {
        arch_sleep();   /* We can safely enter sleep mode here */
    }

    /* Never reached */
    abort();
}

/* Reaper function. This function is here to reap old zombie threads as they are
   created. */
static void *thd_reaper(void *param) {
    kthread_t *thd, *tmp;

    (void)param;

    for(;;) {
        /* Wait til we have something to reap */
        sem_wait(&thd_reap_sem);

        /* Find the first zombie thread and reap it (only do one at a time so
           that the semaphore stays current) */
        LIST_FOREACH_SAFE(thd, &thd_list, t_list, tmp) {
            if(thd->state == STATE_ZOMBIE) {
                thd_destroy(thd);
                break;
            }
        }
    }

    /* Never reached */
    abort();
}

/* Thread execution wrapper; when the thd_create function below
   adds a new thread to the thread chain, this function is the one
   that gets called in the new context. */
static void thd_birth(void *(*routine)(void *param), void *param) {
    /* Call the thread function */
    void *rv = routine(param);

    /* Die */
    thd_exit(rv);
}

/* Terminate the current thread */
void thd_exit(void *rv) {
    /* The thread's never coming back so we don't need to bother saving the
       interrupt state at all. Disable interrupts just to make sure nothing
       changes underneath us while we're doing our thing here */
    irq_disable();

    /* Set the return value of the thread */
    thd_current->rv = rv;

    /* Call newlib's thread cleanup function */
    _reclaim_reent(&thd_current->thd_reent);

    if(thd_current->flags & THD_DETACHED) {
        /* Call Dr. Kevorkian; after this executes we could be killed
           at any time. */
        thd_current->state = STATE_ZOMBIE;
        sem_signal(&thd_reap_sem);
    }
    else {
        /* Mark the thread as finished and wake up anyone that has tried to join
           with it */
        thd_current->state = STATE_FINISHED;
        genwait_wake_all(thd_current);
    }

    /* Manually reschedule */
    thd_block_now(&thd_current->context);

    /* not reached */
    abort();
}


/*****************************************************************************/
/* Thread creation and deletion */

/* Enqueue a process in the runnable queue; adds it right after the
   process group of the same priority (front_of_line==0) or
   right before the process group of the same priority (front_of_line!=0).
   See thd_schedule for why this is helpful. */
void thd_add_to_runnable(kthread_t *t, bool front_of_line) {
    kthread_t *i;
    int done;

    if(t->flags & THD_QUEUED)
        return;

    done = 0;

    if(!front_of_line) {
        /* Look for a thread of lower priority and insert
           before it. If there is nothing on the run queue, we'll
           fall through to the bottom. */
        TAILQ_FOREACH(i, &run_queue, thdq) {
            if(i->prio > t->prio) {
                TAILQ_INSERT_BEFORE(i, t, thdq);
                done = 1;
                break;
            }
        }
    }
    else {
        /* Look for a thread of the same or lower priority and
           insert before it. If there is nothing on the run queue,
           we'll fall through to the bottom. */
        TAILQ_FOREACH(i, &run_queue, thdq) {
            if(i->prio >= t->prio) {
                TAILQ_INSERT_BEFORE(i, t, thdq);
                done = 1;
                break;
            }
        }
    }

    /* Didn't find one, put it at the end */
    if(!done)
        TAILQ_INSERT_TAIL(&run_queue, t, thdq);

    t->flags |= THD_QUEUED;
}

/* Removes a thread from the runnable queue, if it's there. */
int thd_remove_from_runnable(kthread_t *thd) {
    if(!(thd->flags & THD_QUEUED)) return 0;

    thd->flags &= ~THD_QUEUED;
    TAILQ_REMOVE(&run_queue, thd, thdq);
    return 0;
}

/* Creates and initializes the static TLS segment for a thread,
   composed of a Thread Control Block (TCB), followed by .TDATA,
   followed by .TBSS, very carefully ensuring alignment of each
   subchunk. 
*/
static void *thd_create_tls_data(void) {
    size_t align, tdata_offset, tdata_end, tbss_offset, 
        tbss_end, align_rem, tls_size;
    
    tcbhead_t *tcbhead;
    void *tdata_segment, *tbss_segment;

    /* Cached and typed local copies of TLS segment data for sizes, 
       alignments, and initial value data pointer, exported by the 
       linker script. 

       SIZES MUST BE VOLATILE or the optimizer on non-debug builds will 
       optimize zero-check conditionals away, since why would the 
       address of a variable be NULL? (Linker script magic, it can be.)
   */
    const volatile size_t   tdata_size  = (size_t)(&_tdata_size);
    const volatile size_t   tbss_size   = (size_t)(&_tbss_size);
    const          size_t   tdata_align = tdata_size ? (size_t)_tdata_align : 1;
    const          size_t   tbss_align  = tbss_size ? (size_t)_tbss_align : 1;
    const          uint8_t *tdata_start = (const uint8_t *)(&_tdata_start);

    /* Each subsegment of the requested memory chunk must be aligned
       by the largest segment's memory alignment requirements. 
   */
    align = 8;               /* tcbhead_t has to be aligned by 8. */
    if(tdata_align > align)
        align = tdata_align; /* .TDATA segment's alignment */
    if(tbss_align > align)
        align = tbss_align;  /* .TBSS segment's alignment */

    /* Calculate the sizing and offset location of each subsegment. */
    tdata_offset = align_to(sizeof(tcbhead_t), align);
    tdata_end    = tdata_offset + tdata_size;
    tbss_offset  = align_to(tdata_end, tbss_align);
    tbss_end     = tbss_offset + tbss_size; 

    /* Calculate final aligned size requirement. */
    align_rem = tbss_end % align;
    tls_size  = tbss_end;

    if(align_rem)
        tls_size += (align - align_rem);

    /* Allocate combined chunk with calculated size and alignment.  */
    tcbhead = memalign(align, tls_size);
    assert(tcbhead);    
    assert(!((uintptr_t)tcbhead % 8)); 

    /* Since we aren't using either member within it, zero out tcbhead. */
    memset(tcbhead, 0, sizeof(tcbhead_t));

    /* Initialize .TDATA */
    if(tdata_size) { 
        tdata_segment = (uint8_t *)tcbhead + tdata_offset;

        /* Verify proper alignment. */   
        assert(!((uintptr_t)tdata_segment % tdata_align));

        /* Initialize tdata_segment with .tdata bytes from ELF. */
        memcpy(tdata_segment, tdata_start, tdata_size);
    }

    /* Initialize .TBSS */
    if(tbss_size) { 
        tbss_segment = (uint8_t *)tcbhead + tbss_offset; 

        /* Verify proper alignment. */
        assert(!((uintptr_t)tbss_segment % tbss_align));
           
        /* Zero-initialize tbss_segment. */
        memset(tbss_segment, 0, tbss_size);
    }

    /* Return segment head: this is what GBR points to. */
    return tcbhead;
}

/* New thread function; given a routine address, it will create a
   new kernel thread with the given attributes. When the routine
   returns, the thread will exit. Returns the new thread struct. */
kthread_t *thd_create_ex(const kthread_attr_t *restrict attr,
                         void *(*routine)(void *param), void *param) {
    kthread_t *nt = NULL;
    tid_t tid;
    uint32_t params[4];
    int oldirq = 0;
    kthread_attr_t real_attr = { false, THD_STACK_SIZE, NULL, PRIO_DEFAULT, NULL };

    if(attr)
        real_attr = *attr;

    /* Look through the attributes and see what we have. If any are set to 0,
       then default them now to save ourselves trouble later. */
    if(real_attr.stack_ptr && !real_attr.stack_size) {
        errno = EINVAL;
        return NULL;
    }

    if(!real_attr.stack_size)
        real_attr.stack_size = THD_STACK_SIZE;

    if(!real_attr.prio)
        real_attr.prio = PRIO_DEFAULT;

    oldirq = irq_disable();

    /* Get a new thread id */
    tid = thd_next_free();

    if(tid >= 0) {
        /* Create a new thread structure */
        nt = memalign(32, sizeof(kthread_t));

        if(nt != NULL) {
            /* Clear out potentially unused stuff */
            memset(nt, 0, sizeof(kthread_t));

            /* Initialize the flags to defaults immediately. */
            nt->flags = THD_DEFAULTS;

            /* Create a new thread stack */
            if(!real_attr.stack_ptr) {
                nt->stack = (uint32_t*)malloc(real_attr.stack_size);

                if(!nt->stack) {
                    free(nt);
                    irq_restore(oldirq);
                    return NULL;
                }

                /* Since we allocated the stack, we own the stack! */
                nt->flags |= THD_OWNS_STACK;
            }
            else {
                nt->stack = (uint32_t*)real_attr.stack_ptr;
            }

            nt->stack_size = real_attr.stack_size;

            /* Create static TLS data */
            nt->tcbhead = thd_create_tls_data();

            /* Populate the context */
            params[0] = (uint32_t)routine;
            params[1] = (uint32_t)param;
            params[2] = 0;
            params[3] = 0;
            irq_create_context(&nt->context,
                               ((uint32_t)nt->stack) + nt->stack_size,
                               (uint32_t)thd_birth, params, 0);

            /* Set Thread Pointer */
            nt->context.gbr = (uint32_t)nt->tcbhead;
            nt->tid = tid;
            nt->prio = real_attr.prio;
            nt->state = STATE_READY;

            if(!real_attr.label) {
                strcpy(nt->label, "[un-named kernel thread]");
            }
            else {
                strncpy(nt->label, real_attr.label, 255);
                nt->label[255] = 0;
            }

            if(thd_current)
                strcpy(nt->pwd, thd_current->pwd);
            else
                strcpy(nt->pwd, "/");

            _REENT_INIT_PTR((&(nt->thd_reent)));

            /* Should we detach the thread? */
            if(real_attr.create_detached)
                nt->flags |= THD_DETACHED;

            /* Initialize thread-local storage. */
            LIST_INIT(&nt->tls_list);

            /* Insert it into the thread list */
            LIST_INSERT_HEAD(&thd_list, nt, t_list);

            /* Add it to our count */
            ++thd_count;

            /* Schedule it */
            thd_add_to_runnable(nt, 0);
        }
    }

    irq_restore(oldirq);
    return nt;
}

kthread_t *thd_create(bool detach, void *(*routine)(void *), void *param) {
    kthread_attr_t attrs = { detach, 0, NULL, 0, NULL };
    return thd_create_ex(&attrs, routine, param);
}

/* Given a thread id, this function removes the thread from
   the execution chain. */
int thd_destroy(kthread_t *thd) {
    int oldirq = 0;
    kthread_tls_kv_t *i, *i2;

    /* Make sure there are no ints */
    oldirq = irq_disable();

    /* If any threads were waiting on this one, then go ahead
       and unblock them. */
    genwait_wake_all(thd);

    /* If this thread was waiting on something, we need to remove it from
       genwait so that it doesn't try to notify a dead thread later. */
    if(thd->wait_obj)
        genwait_wake_thd(thd->wait_obj, thd, ECANCELED);

    /* De-schedule the thread if it's scheduled. */
    thd_remove_from_runnable(thd);

    /* Remove it from the thread list. */
    LIST_REMOVE(thd, t_list);

    /* Call destructors on TLS entries.  */
    LIST_FOREACH(i, &thd->tls_list, kv_list) {
        if(i->destructor) {
            i->destructor(i->data);
        }
    }

    /* Free TLS entries. */
    i = LIST_FIRST(&thd->tls_list);
    while(i != NULL) {
        i2 = LIST_NEXT(i, kv_list);
        free(i);
        i = i2;
    }

    /* Free its stack (if we're managing it). */
    if(thd->flags & THD_OWNS_STACK)
        free(thd->stack);

    /* Free static TLS segment */
    free(thd->tcbhead);

    /* Free the thread */
    free(thd);

    /* Remove it from the count */
    --thd_count;

    /* Put ints back the way they were */
    irq_restore(oldirq);

    return 0;
}

/*****************************************************************************/
/* Thread attribute functions */

/* Set a thread's priority */
int thd_set_prio(kthread_t *thd, prio_t prio) {
    if(thd == NULL)
        return -1;

    if((prio < 0) || (prio > PRIO_MAX))
        return -2;

    /* Set the new priority */
    thd->prio = prio;
    return 0;
}

/*****************************************************************************/
/* Scheduling routines */

static void thd_update_cpu_time(kthread_t *thd) {
    const uint64_t ns = perf_cntr_timer_ns();

    thd_current->cpu_time.total +=
            ns - thd_current->cpu_time.scheduled;

    thd->cpu_time.scheduled = ns;
}

/* Thread scheduler; this function will find a new thread to run when a
   context switch is requested. No work is done in here except to change
   out the thd_current variable contents. Assumed that we are in an
   interrupt context.

   In the normal operation mode, the current thread is pushed back onto
   the run queue at the end of its priority group. This implements the
   standard round robin scheduling within priority groups. If you set the
   front_of_line parameter to non-zero, then this behavior is modified:
   the current thread is pushed onto the run queue at the _front_ of its
   priority group. The effect is that no context switching is done, but
   priority groups are re-checked. This is useful when returning from an
   IRQ after doing something like a sem_signal, where you'd ideally like
   to make sure the priorities are all straight before returning, but you
   don't want a full context switch inside the same priority group.
*/
void thd_schedule(bool front_of_line, uint64_t now) {
    kthread_t *thd;

    if(now == 0)
        now = timer_ms_gettime64();

    /* If there's only two thread left, it's the idle task and the reaper task:
       exit the OS */
    if(thd_count == 2) {
        dbgio_printf("\nthd_schedule: idle tasks are the only things left; exiting\n");
        arch_exit();
    }

    /* If the current thread is supposed to be in the front of the line, and it
       did not die, re-enqueue it to the front of the line now. */
    if(front_of_line && thd_current->state == STATE_RUNNING) {
        thd_current->state = STATE_READY;
        thd_add_to_runnable(thd_current, front_of_line);
    }

    /* Look for timed out waits */
    genwait_check_timeouts(now);

    /* Search downwards through the run queue for a runnable thread; if
       we don't find a normal runnable thread, the idle process will
       always be there at the bottom. */
    TAILQ_FOREACH(thd, &run_queue, thdq) {
        /* Is it runnable? If not, keep going */
        if(thd->state == STATE_READY)
            break;
    }

    /* If we didn't already re-enqueue the thread and we are supposed to do so,
       do it now. */
    if(!front_of_line && thd_current->state == STATE_RUNNING) {
        thd_current->state = STATE_READY;
        thd_add_to_runnable(thd_current, front_of_line);

        /* Make sure we have a thread, just in case we couldn't find anything
           above. */
        if(thd == NULL || thd == thd_idle_thd)
            thd = thd_current;
    }

    /* Didn't find one? Big problem here... */
    if(thd == NULL) {
        thd_pslist(printf);
        arch_panic("couldn't find a runnable thread");
    }

    /* We should now have a runnable thread, so remove it from the
       run queue and switch to it. */
    thd_remove_from_runnable(thd);

    thd_update_cpu_time(thd);

    thd_current = thd;
    _impure_ptr = &thd->thd_reent;
    thd->state = STATE_RUNNING;

    /* Make sure the thread hasn't underrun its stack */
    if(thd_current->stack && thd_current->stack_size) {
        if(CONTEXT_SP(thd_current->context) < (ptr_t)(thd_current->stack)) {
            thd_pslist(printf);
            thd_pslist_queue(printf);
            assert_msg(0, "Thread stack underrun");
        }
    }

    irq_set_context(&thd_current->context);
}

/* Temporary priority boosting function: call this from within an interrupt
   to boost the given thread to the front of the queue. This will cause the
   interrupt return to jump back to the new thread instead of the one that
   was executing (unless it was already executing). */
void thd_schedule_next(kthread_t *thd) {
    /* Make sure we're actually inside an interrupt */
    if(!irq_inside_int())
        return;

    /* We're already running now! */
    if(thd == thd_current)
        return;

    /* Can't boost a blocked thread */
    if(thd->state != STATE_READY)
        return;

    /* Unfortunately we have to take care of this here */
    if(thd_current->state == STATE_ZOMBIE) {
        sem_signal(&thd_reap_sem);
    }
    else if(thd_current->state == STATE_RUNNING) {
        thd_current->state = STATE_READY;
        thd_add_to_runnable(thd_current, 0);
    }

    thd_remove_from_runnable(thd);

    thd_update_cpu_time(thd);

    thd_current = thd;
    _impure_ptr = &thd->thd_reent;
    thd_current->state = STATE_RUNNING;
    irq_set_context(&thd_current->context);
}

/* See kos/thread.h for description */
irq_context_t *thd_choose_new(void) {
    uint64_t now = timer_ms_gettime64();

    //printf("thd_choose_new() woken at %d\n", (uint32_t)now);

    /* Do any re-scheduling */
    thd_schedule(0, now);

    /* Return the new IRQ context back to the caller */
    return &thd_current->context;
}

/*****************************************************************************/

/* Timer function. Check to see if we were woken because of a timeout event
   or because of a preempt. For timeouts, just go take care of it and sleep
   again until our next context switch (if any). For pre-empts, re-schedule
   threads, swap out contexts, and sleep. */
static void thd_timer_hnd(irq_context_t *context) {
    /* Get the system time */
    uint64_t now = timer_ms_gettime64();

    (void)context;

    //printf("timer woke at %d\n", (uint32_t)now);

    thd_schedule(0, now);
    timer_primary_wakeup(thd_sched_ms);
}

/*****************************************************************************/

/* Thread blocking based sleeping; this is the preferred way to
   sleep because it eases the load on the system for the other
   threads. */
void thd_sleep(unsigned int ms) {
    /* This should never happen. This should, perhaps, assert. */
    if(thd_mode == THD_MODE_NONE) {
        dbglog(DBG_WARNING, "thd_sleep called when threading not "
               "initialized.\n");
        timer_spin_sleep(ms);
        return;
    }

    /* A timeout of zero is the same as thd_pass() and passing zero
       down to genwait_wait() causes bad juju. */
    if(!ms) {
        thd_pass();
        return;
    }

    /* We can genwait on a non-existent object here with a timeout and
       have the exact same effect; as a nice bonus, this collapses both
       sleep cases into a single case, which is nice for scheduling
       purposes. 0xffffffff definitely doesn't exist as an object, so we'll
       use that for straight up timeouts. */
    genwait_wait((void *)0xffffffff, "thd_sleep", ms, NULL);
}

/* Manually cause a re-schedule */
void thd_pass(void) {
    /* Makes no sense inside int */
    if(irq_inside_int()) return;

    /* Pass off control manually */
    thd_block_now(&thd_current->context);
}

/* Wait for a thread to exit */
int thd_join(kthread_t *thd, void **value_ptr) {
    int old, rv;
    kthread_t * t = NULL;

    /* Can't scan for NULL threads */
    if(thd == NULL)
        return -1;

    /* If you wait for yourself, you'll never leave */
    if(thd == thd_current)
        return -4;

    if((rv = irq_inside_int())) {
        dbglog(DBG_WARNING, "thd_join(%p) called inside an interrupt with "
               "code: %x evt: %.4x\n", (void *)thd, ((rv >> 16) & 0xf),
               (rv & 0xffff));
        return -1;
    }

    old = irq_disable();

    /* Search the thread list and make sure that this thread hasn't
       already died and been deallocated. */
    LIST_FOREACH(t, &thd_list, t_list) {
        if(t == thd)
            break;
    }

    /* Did we find anything? */
    if(t != thd) {
        rv = -2;
    }
    else if((thd->flags & THD_DETACHED)) {
        /* Can't join a detached thread */
        rv = -3;
    }
    else {
        if(thd->state != STATE_FINISHED) {
            /* Wait for the target thread to die */
            genwait_wait(thd, "thd_join", 0, NULL);
        }

        /* Ok, we're all clear */
        rv = 0;

        if(value_ptr)
            *value_ptr = thd->rv;

        /* The thread can be destroyed now */
        thd_destroy(thd);
    }

    irq_restore(old);
    return rv;
}

/* Detach a joinable thread */
int thd_detach(kthread_t *thd) {
    int old, rv = 0;
    kthread_t * t = NULL;

    /* Can't scan for NULL threads */
    if(thd == NULL)
        return -1;

    old = irq_disable();

    /* Search the thread list and make sure that this thread hasn't
       already died and been deallocated. */
    LIST_FOREACH(t, &thd_list, t_list) {
        if(t == thd)
            break;
    }

    /* Did we find anything? */
    if(t != thd) {
        rv = -2;
    }
    else if(thd->flags & THD_DETACHED) {
        /* Can't detach an already detached thread */
        rv = -3;
    }
    else if(thd->state == STATE_FINISHED) {
        /* If the thread is already finished, deallocate it now */
        thd_destroy(thd);
    }
    else {
        /* Set the detached flag and return */
        thd->flags |= THD_DETACHED;
    }

    irq_restore(old);
    return rv;
}


/*****************************************************************************/
/* Retrieve / set thread label */
const char *thd_get_label(kthread_t *thd) {
    return thd->label;
}

void thd_set_label(kthread_t *thd, const char *restrict label) {
    strncpy(thd->label, label, sizeof(thd->label) - 1);
}

/* Find the current thread */
kthread_t *thd_get_current(void) {
    return thd_current;
}

/* Retrieve / set thread pwd */
const char *thd_get_pwd(kthread_t *thd) {
    return thd->pwd;
}

void thd_set_pwd(kthread_t *thd, const char *restrict pwd) {
    strncpy(thd->pwd, pwd, sizeof(thd->pwd) - 1);
}

int *thd_get_errno(kthread_t *thd) {
    return &thd->thd_errno;
}

struct _reent *thd_get_reent(kthread_t *thd) {
    return &thd->thd_reent;
}

uint64_t thd_get_cpu_time(kthread_t *thd) {
    /* Check whether we should force an update immediately for accuracy. */
    if(thd == thd_get_current())
        thd_update_cpu_time(thd);

    return thd->cpu_time.total;
}

/*****************************************************************************/

/* Change threading modes */
int thd_set_mode(kthread_mode_t mode) {
    dbglog(DBG_WARNING, "thd_set_mode() has no effect. Cooperative threading "
           "mode is deprecated. Threading is always in preemptive mode.\n");

    return mode;
}

kthread_mode_t thd_get_mode(void) {
    return thd_mode;
}

unsigned thd_get_hz(void) {
    return 1000 / thd_sched_ms;
}

int thd_set_hz(unsigned int hertz) {
    if(!hertz || hertz > 1000)
        return -1;

    thd_sched_ms = 1000 / hertz;

    return 0;
}

/* Delete a TLS key. Note that currently this doesn't prevent you from reusing
   the key after deletion. This seems ok, as the pthreads standard states that
   using the key after deletion results in "undefined behavior".
   XXXX: This should really be in tls.c, but we need the list of threads to go
   through, so it ends up here instead. */
int kthread_key_delete(kthread_key_t key) {
    int old = irq_disable();
    kthread_t *cur;
    kthread_tls_kv_t *i, *tmp;

    /* Make sure the key is valid. */
    if(key >= kthread_key_next() || key < 1) {
        irq_restore(old);
        errno = EINVAL;
        return -1;
    }

    /* Make sure we can actually use free below. */
    if(!malloc_irq_safe()) {
        irq_restore(old);
        errno = EPERM;
        return -1;
    }

    /* Go through each thread searching for (and removing) the data. */
    LIST_FOREACH(cur, &thd_list, t_list) {
        LIST_FOREACH_SAFE(i, &cur->tls_list, kv_list, tmp) {
            if(i->key == key) {
                LIST_REMOVE(i, kv_list);
                free(i);
                break;
            }
        }
    }

    kthread_key_delete_destructor(key);

    irq_restore(old);
    return 0;
}

/*****************************************************************************/
/* Init/shutdown */

/* Init */
int thd_init(void) {
    const kthread_attr_t kern_attr = {
        .stack_size = THD_KERNEL_STACK_SIZE,
        .stack_ptr  = (void *)_arch_mem_top - THD_KERNEL_STACK_SIZE,
        .label      = "[kernel]"
    };
    kthread_t *kern, *reaper;

    /* Make sure we're not already running */
    if(thd_mode != THD_MODE_NONE)
        return -1;

    /* Setup our mode as appropriate */
    thd_mode = THD_MODE_PREEMPT;

    /* Initialize handle counters */
    tid_highest = 1;

    /* Initialize the thread list */
    LIST_INIT(&thd_list);

    /* Initialize the run queue */
    TAILQ_INIT(&run_queue);

    /* Start off with no "current" thread */
    thd_current = NULL;

    /* Init thread-local storage. */
    kthread_tls_init();

    /* Reinitialize thread counter */
    thd_count = 0;

    /* Setup a kernel task for the currently running "main" thread */
    kern = thd_create_ex(&kern_attr, NULL, NULL);
    kern->state = STATE_RUNNING;

    /* Initialize GBR register for Main Thread */
    __builtin_set_thread_pointer((void*)kern->context.gbr);

    /* De-scehdule the thread (it's STATE_RUNNING) */
    thd_remove_from_runnable(kern);

    /* Setup an idle task that is always ready to run, in case everyone
       else is blocked on something. */
    thd_idle_thd = thd_create(0, thd_idle_task, NULL);
    strcpy(thd_idle_thd->label, "[idle]");
    thd_set_prio(thd_idle_thd, PRIO_MAX);
    thd_idle_thd->state = STATE_READY;

    /* Set up a thread to reap old zombies */
    sem_init(&thd_reap_sem, 0);
    reaper = thd_create(0, thd_reaper, NULL);
    strcpy(reaper->label, "[reaper]");
    thd_set_prio(reaper, 1);

    /* Main thread -- the kern thread */
    thd_current = kern;
    irq_set_context(&kern->context);

    thd_update_cpu_time(thd_current);

    /* Initialize thread sync primitives */
    genwait_init();

    /* Setup our pre-emption handler */
    timer_primary_set_callback(thd_timer_hnd);

    /* Schedule our first wakeup */
    timer_primary_wakeup(thd_sched_ms);

    dbglog(DBG_DEBUG, "thd: pre-emption enabled, HZ=%u\n", thd_get_hz());

    return 0;
}

/* Shutdown */
void thd_shutdown(void) {
    kthread_t *cur, *tmp;

    /* Remove our pre-emption handler */
    timer_primary_set_callback(NULL);

    /* Kill remaining live threads */
    LIST_FOREACH_SAFE(cur, &thd_list, t_list, tmp) {
        thd_destroy(cur);
    }

    sem_destroy(&thd_reap_sem);

    /* Shutdown thread sync primitives */
    genwait_shutdown();

    kthread_tls_shutdown();

    /* Not running */
    thd_mode = THD_MODE_NONE;
    thd_count = 0;

    // XXX _impure_ptr is borked
}
