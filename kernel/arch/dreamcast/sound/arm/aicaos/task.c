/* KallistiOS ##version##

   task.c
   Copyright (C) 2024 Paul Cercueil

   Simple threading system for ARM
*/

#include <aicaos/aica.h>
#include <aicaos/irq.h>
#include <aicaos/task.h>
#include <stddef.h>

static unsigned char counter_channel;

static struct task idle_task;
struct task *current_task;

static unsigned short last_pos;
static unsigned int task_counter = 0;
static struct task * tasks[TASK_PRIO_COUNT];

/* Inside task_asm.S */
__noreturn void task_select(struct context *context);
__noreturn void task_exit(void);

static void idle_function(void)
{
    /* TODO: can the processor actually sleep? */

    for (;;)
        task_reschedule();
}

void aica_init_tasks(void)
{
    task_init(&idle_task, "idle", idle_function, NULL, TASK_PRIO_IDLE, NULL, 0);

    /* Use a regular channel as a counter.
     * Since timers are not readable, we use this channel to read how much time
     * has elapsed since the last time a timer was programmed. */
    counter_channel = 63;
    counter_init(counter_channel);
}

unsigned short task_read_counter(void)
{
    return aica_get_pos(counter_channel);
}

static void task_wakeup(unsigned short ticks)
{
    struct task *task;
    unsigned int i;

    for (i = 0; i < TASK_PRIO_COUNT; i++) {
        for (task = tasks[i]; task; task = task->next) {
            if (task->state == TASK_SLEEPING) {
                if (task->wakeup > ticks)
                    task->wakeup -= ticks;
                else
                    task->state = TASK_RUNNING;
            }
        }
    }
}

static void task_program_next_wakeup(void)
{
    unsigned int i, div = 0, wakeup = DEFAULT_TIMEOUT_WAKEUP;
    struct task *task;

    for (i = 0; i < TASK_PRIO_COUNT; i++) {
        for (task = tasks[i]; task; task = task->next)
            if (task->state == TASK_RUNNING)
                break;

        if (task)
            break;

        for (task = tasks[i]; task; task = task->next) {
            if (task->state == TASK_SLEEPING && task->wakeup < wakeup)
                wakeup = task->wakeup;
        }
    }

    while (wakeup > 255) {
        wakeup >>= 1;
        div++;
    }

    /* Re-program the timer to the next event */
    SPU_REG32(REG_SPU_TIMER0_CTRL) =
        SPU_FIELD_PREP(SPU_TIMER_CTRL_START, 256 - wakeup) |
        SPU_FIELD_PREP(SPU_TIMER_CTRL_DIV, div);

    /* Re-enable timer */
    SPU_REG32(REG_SPU_INT_RESET) = SPU_INT_ENABLE_TIMER0;
}

static __noreturn void __task_select(struct task *task)
{
    irq_disable();

    current_task = task;
    task_select(&task->context);
}

/* Called from task_asm.S */
__noreturn void __task_reschedule(_Bool skip_me)
{
    unsigned short counter, ticks;
    struct task *task;
    unsigned int i;

    irq_disable();

    /* Cancel previous wakeup timer */
    SPU_REG32(REG_SPU_TIMER0_CTRL) = 0;

    counter = task_read_counter();
    ticks = counter - last_pos;
    last_pos = counter;

    /* Wake up sleeping tasks, and program next wakeup */
    task_wakeup(ticks);
    task_program_next_wakeup();

    for (i = 0; i < TASK_PRIO_COUNT; i++) {
        for (task = tasks[i]; task; task = task->next) {
            if (skip_me && task == current_task)
                continue;

            if (task->state == TASK_RUNNING)
                __task_select(task);
        }
    }

    __builtin_unreachable();
}

/* Called from task_asm.S */
__noreturn void __task_exit(void)
{
    struct task *task, *prev = NULL;
    unsigned int i;

    irq_disable();

    for (i = 0; i < TASK_PRIO_COUNT; i++) {
        for (task = tasks[i]; task; prev = task, task = task->next) {
            if (task == current_task) {
                /* Remove the task from the runnables list */
                if (prev)
                    prev->next = task->next;
                else
                    tasks[i] = task->next;

                task->state = TASK_DEAD;
                break;
            }
        }

        if (task)
            break;
    }

    __task_reschedule(0);
}

void task_init(struct task *task, const char *name, void *func,
               unsigned int params[4], enum task_prio prio,
               unsigned int *stack, unsigned int stack_size)
{
    unsigned int i, cxt;

    for (i = 0; params && i < 4; i++)
        task->context.r0_r7[i] = params[i];

    /* Set the stack pointer */
    task->context.r8_r14[5] = (unsigned int)stack + stack_size;
    task->context.r8_r14[6] = (unsigned int)task_exit;
    task->context.pc = (unsigned int)func + 4;
    task->context.cpsr = 0x13; /* supervisor */
    task->state = TASK_RUNNING;
    task->name = name;

    cxt = irq_disable();
    task->id = task_counter++;

    task->next = tasks[prio];
    tasks[prio] = task;

    irq_restore(cxt);
}

void task_sleep(ticks_t ticks)
{
    if (ticks) {
        current_task->wakeup = ticks;
        current_task->state = TASK_SLEEPING;

        task_reschedule();
    }
}
