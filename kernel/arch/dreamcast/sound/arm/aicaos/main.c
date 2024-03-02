#include <aicaos/aica.h>
#include <aicaos/irq.h>
#include <cmd_iface.h>
#include <stddef.h>

extern int main(int argc, char **argv);

extern unsigned char __heap_start, __heap_end;

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
    /* Initialize the AICA part of the SPU */
    aica_init();

    aica_interrupt_init();

    aica_header.buffer_size =
        (unsigned int)&__heap_end - (unsigned int)&__heap_start;

    /* Set header pointer, so that the SH4 knows where the header is */
    *(struct aica_header **)AICA_HEADER_ADDR = &aica_header;

    main(0, NULL);
}
