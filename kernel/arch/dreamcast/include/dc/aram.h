/* KallistiOS ##version##

   aram.h
   Copyright (C) 2024 Paul Cercueil

*/

/** \file    dc/aram.h
  \brief   Sound RAM macros and routines.
  \ingroup system_aram

  \author Paul Cercueil
  */

#ifndef __DC_ARAM_H
#define __DC_ARAM_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <stdint.h>
#include <dc/g2bus.h>
#include <dc/spu.h>

typedef uint32_t aram_addr_t;

static inline void * aram_addr_to_host(aram_addr_t addr)
{
	return (void *)(addr + SPU_RAM_UNCACHED_BASE);
}

static inline uint8_t aram_read_8(aram_addr_t addr)
{
    g2_fifo_wait();
    return g2_read_8(addr + SPU_RAM_UNCACHED_BASE);
}

static inline uint32_t aram_read_32(aram_addr_t addr)
{
    g2_fifo_wait();
    return g2_read_32(addr + SPU_RAM_UNCACHED_BASE);
}

static inline void aram_write_8(aram_addr_t addr, uint8_t val)
{
    g2_fifo_wait();
    g2_write_8(addr + SPU_RAM_UNCACHED_BASE, val);
}

static inline void aram_write_16(aram_addr_t addr, uint16_t val)
{
    g2_write_16(addr + SPU_RAM_UNCACHED_BASE, val);
}

static inline void aram_write_32(aram_addr_t addr, uint32_t val)
{
    g2_fifo_wait();
    g2_write_32(addr + SPU_RAM_UNCACHED_BASE, val);
}

void aram_read(void *out, aram_addr_t addr, size_t size);
void aram_write(aram_addr_t dst, const void *src, size_t size);

__END_DECLS

#endif  /* __DC_ARAM_H */
