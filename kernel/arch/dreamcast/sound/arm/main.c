/* KallistiOS ##version##

   main.c
   (c)2000-2002 Megan Potter

   Generic sound driver with streaming capabilities

   This slightly more complicated version allows for sound effect channels,
   and full sampling rate, panning, and volume control for each.

*/

#include "aica_cmd_iface.h"
#include "aica_registers.h"
#include "aica.h"
#include "irq.h"
#include "mm.h"
#include "queue.h"
#include "task.h"

#include <stddef.h>

extern unsigned char __heap_start, __heap_end;
extern volatile unsigned int timer;

static struct task main_task;
static unsigned int main_task_stack[0x400];

static char command_buffer[0x10000];
static char response_buffer[0x10000];
static struct aica_channel channels[64];

static struct aica_queue command_queue = {
    .data = (uint32)command_buffer,
    .size = sizeof(command_buffer),
    .valid = 1,
    .process_ok = 1,
};

static struct aica_queue response_queue = {
    .data = (uint32)response_buffer,
    .size = sizeof(response_buffer),
    .valid = 1,
    .process_ok = 1,
};

struct aica_header aica_header = {
    .cmd_queue = &command_queue,
    .resp_queue = &response_queue,
    .channels = channels,
    .buffer = &__heap_start,
};

/****************** Main Program ************************************/

static void arm_main_task(void)
{
    unsigned int i;

    aica_printf("AICA firmware initialized.\n");

    /* Wait for a command */
    for (;;) {
        /* Update channel position counters */
        for(i = 0; i < 64; i++)
            aica_header.channels[i].pos = aica_get_pos(i);

        /* Little delay to prevent memory lock */
        task_sleep(ms_to_ticks(10));
    }
}

__noreturn void arm_main(void)
{
    /* Initialize the AICA part of the SPU */
    aica_init();
    aica_interrupt_init();
    aica_init_tasks();

    /* Initialize the communication queues */
    aica_init_queue(&aica_header);

    /* Initialize the memory manager */
    aica_mm_init();

    aica_header.buffer_size =
        (unsigned int)&__heap_end - (unsigned int)&__heap_start;

    /* Set header pointer, so that the SH4 knows where the header is */
    *(struct aica_header **)AICA_HEADER_ADDR = &aica_header;

    /* Register and add our main task */
    task_init(&main_task, arm_main_task, NULL, TASK_PRIO_LOW,
              main_task_stack, sizeof(main_task_stack));

    __task_reschedule(0);
}
