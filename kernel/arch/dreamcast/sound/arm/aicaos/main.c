#include <aicaos/aica.h>
#include <aicaos/irq.h>
#include <aicaos/queue.h>
#include <aicaos/task.h>
#include <cmd_iface.h>
#include <stddef.h>

extern int main(int argc, char **argv);

extern unsigned char __heap_start, __heap_end;

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

/* Initialize the OS */
void arm_main(void)
{
    unsigned int args[4] = { 0 };

    /* Initialize the AICA part of the SPU */
    aica_init();

    aica_interrupt_init();
    aica_init_tasks();
    aica_init_queue(&aica_header);

    aica_header.buffer_size =
        (unsigned int)&__heap_end - (unsigned int)&__heap_start;

    /* Set header pointer, so that the SH4 knows where the header is */
    *(struct aica_header **)AICA_HEADER_ADDR = &aica_header;

    /* Register and add our main task */
    task_init(&main_task, "main", main, args, TASK_PRIO_LOW,
              main_task_stack, sizeof(main_task_stack));

    current_task = &main_task;
    __task_reschedule(0);
}
