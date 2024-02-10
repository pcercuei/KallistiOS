/* KallistiOS ##version##

   aica_cmd_iface.h
   (c)2000-2002 Megan Potter

   Definitions for the SH-4/AICA interface. This file is meant to be
   included from both the ARM and SH-4 sides of the fence.
*/

#ifndef __ARM_AICA_CMD_IFACE_H
#define __ARM_AICA_CMD_IFACE_H

#include "aica_comm.h"

struct aica_header {
    struct aica_queue   *cmd_queue;
    struct aica_queue   *resp_queue;
    struct aica_channel *channels;
    void                *buffer;
    unsigned int        buffer_size;
};

extern struct aica_header aica_header;

/* This is where our SH-4/AICA comm variables go... */

/* The address of the firmware header will be placed at that address */
#define AICA_HEADER_ADDR    0x1ffffc

#endif  /* __ARM_AICA_CMD_IFACE_H */
