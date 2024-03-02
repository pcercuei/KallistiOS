/* KallistiOS ##version##

   task.c
   Copyright (C) 2024 Paul Cercueil

   Simple threading system for ARM
*/

#include <stddef.h>

#include "task.h"
#include "irq.h"

static struct task idle_task;
struct task *current_task;

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
    task_init(&idle_task, idle_function, NULL, TASK_PRIO_IDLE, NULL, 0);
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
    struct task *task;
    unsigned int i;

    irq_disable();

    for (i = 0; i < TASK_PRIO_COUNT; i++) {
        for (task = tasks[i]; task; task = task->next) {
            if (skip_me && task == current_task)
                continue;

            if (task->state == TASK_RUNNING)
                __task_select(task);
        }
    }

    __task_select(current_task);
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

void task_init(struct task *task, void *func, unsigned int params[4],
               enum task_prio prio, unsigned int *stack,
               unsigned int stack_size)
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

    cxt = irq_disable();
    task->id = task_counter++;

    task->next = tasks[prio];
    tasks[prio] = task;

    irq_restore(cxt);
}
