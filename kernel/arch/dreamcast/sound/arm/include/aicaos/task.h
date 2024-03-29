#ifndef _TASK_H
#define _TASK_H

#define __noreturn __attribute__((noreturn))

#define DEFAULT_STACK_SIZE 4096

/* 1764 ticks of the 44100 Hz clock == 25 Hz */
#define DEFAULT_TIMEOUT_WAKEUP 1764

typedef unsigned int ticks_t;

struct context {
    /* XXX: don't change the order */
    unsigned int r0_r7[8];
    unsigned int pc;
    unsigned int r8_r14[7];
    unsigned int cpsr;
};

enum task_state {
    TASK_DEAD,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_WAIT,
    TASK_WAIT_UNTIL,
};

enum task_prio {
    TASK_PRIO_HIGHEST,
    TASK_PRIO_HIGH,
    TASK_PRIO_NORMAL,
    TASK_PRIO_LOW,
    TASK_PRIO_LOWEST,
    TASK_PRIO_IDLE,
    TASK_PRIO_COUNT,
};

struct task {
    struct context context;
    struct task *next;
    struct task *wait_next;
    _Bool awaken;
    void *wait_obj;
    unsigned int id;
    ticks_t wakeup;
    enum task_state state;
    const char *name;
    enum task_prio prio, real_prio;
};

/* Pointer to the current task */
extern struct task *current_task;

/* Initialize tasks support */
void aica_init_tasks(void);

/* Initialize and start a new task */
void task_init(struct task *task, const char *name, void *func,
               unsigned int params[4], enum task_prio prio,
               unsigned int *stack, unsigned int stack_size);

/* Wait until the given task completes */
void task_join(struct task *task);

/* Request a reschedule. */
void task_reschedule(void);

/* Yield the task.
 * The difference with a reschedule, is that yielding will cause the scheduler
 * to always pick a different task. */
void task_yield(void);

_Bool task_wait_timeout(void *obj, ticks_t ticks);

static inline void task_wait(void *obj)
{
    task_wait_timeout(obj, 0);
}

void task_wake(void *obj, _Bool all);

/* Sleep for a given number of 44100 Hz ticks */
void task_sleep(ticks_t ticks);

/* Convert from microseconds to ticks */
static inline ticks_t us_to_ticks(unsigned int us)
{
    /* Similar as this: cnt = (us * 44100ull + 999999ull) / 1000000ull;
     * But with the constants multiplied by 1.048576 so that we can
     * replace the 64-bit division by a bit shift. */
    return (us * 46242ull + 1048575ull) >> 20;
}

/* Convert from milliseconds to ticks */
static inline ticks_t ms_to_ticks(unsigned int ms)
{
	return us_to_ticks(ms * 1000);
}

/* Read the hardware value of the task counter. */
unsigned short task_read_counter(void);

/* Reschedule without saving the current task. */
__noreturn void __task_reschedule(_Bool skip_me);

/* Boost the given task's priority to match the current task's priority. */
void task_boost(struct task *task);

/* Unboost the current task's priority. */
void task_unboost(void);

#endif
