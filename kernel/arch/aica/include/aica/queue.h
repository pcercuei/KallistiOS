/* KallistiOS ##version##

   aica/irq.h
   Copyright (C) 2026 Paul Cercueil

   RSP mechanism for the AICA
*/

#ifndef __KOS_AICA_QUEUE_H
#define __KOS_AICA_QUEUE_H

#include <kos/regfield.h>
#include <kos/rpc.h>
#include <aica/aica.h>

#include <stdint.h>

typedef enum aica_queue_cmd_code {
    AICA_CMD_OPENFILE,
    AICA_CMD_CLOSEFILE,
    AICA_CMD_READFILE,
    AICA_CMD_WRITEFILE,
    AICA_CMD_SEEKFILE,
    AICA_CMD_TELLFILE,
    AICA_CMD_TOTALFILE,
    AICA_CMD_READDIR,
    AICA_CMD_PUTS,
} aica_queue_cmd_code_t;

/** \brief AICA firmware header

    This structure contains all the information about the firmware that
    both sides need to know: the addresses of the command and response
    queues, the sample buffer address and size, and the channels array
    address.

    On the ARM side, the fields are valid pointers and can be deferred directly.
    On the SH4 side, the fields are ARM addresses (aka. aram_addr_t) and should
    exclusively be manipulated with the ARAM API.
*/
typedef struct aica_header {
    rpc_queue_t   *arm_queue;           /**< Address of the SH4->ARM queue */
    rpc_queue_t   *sh4_queue;           /**< Address of the ARM->SH4 queue */
    aica_chn_data_t    *channels;   /**< Address of the channels array */
} aica_header_t;

/** \brief AICA RPC struct

    This structure is directly accessible from both ARM and SH4 sides.
*/
extern rpc_t aica_rpc;

/** \brief Offset in the sound RAM at which the firmware header address will
    be placed. */
#define AICA_HEADER_ADDR    0x1ffffc

void queue_init(void);
void queue_shutdown(void);

#endif /* __KOS_AICA_QUEUE_H */
