/* KallistiOS ##version##

   pvr_dma.c
   Copyright (C) 2002 Roger Cattermole
   Copyright (C) 2004 Megan Potter
   Copyright (C) 2023 Andy Barajas
   Copyright (C) 2023 Ruslan Rostovtsev

   http://www.boob.co.uk
 */

#include <stdio.h>
#include <errno.h>
#include <dc/pvr.h>
#include <dc/asic.h>
#include <dc/dmac.h>
#include <dc/sq.h>
#include <kos/thread.h>
#include <kos/sem.h>

#include "pvr_internal.h"

/* Modified for inclusion into KOS by Megan Potter */

/* Signaling semaphore */
static semaphore_t dma_done;
static int32_t dma_blocking;
static pvr_dma_callback_t dma_callback;
static void *dma_cbdata;

/* DMA registers */
static vuint32 * const pvr_dma = (vuint32 *)0xa05f6800;

/* PVR Dma registers - Offset by 0xA05F6800 */
#define PVR_STATE   0x00
#define PVR_LEN     0x04/4
#define PVR_DST     0x08/4
#define PVR_LMMODE0 0x84/4
#define PVR_LMMODE1 0x88/4

static void pvr_dma_irq_hnd(uint32_t code, void *) {
    (void)code;

    if(DMAC_DMATCR2 != 0)
        dbglog(DBG_INFO, "pvr_dma: The dma did not complete successfully\n");

    /* Call the callback, if any. */
    if(dma_callback) {
        /* This song and dance is necessary because the handler
           could chain to itself. */
        pvr_dma_callback_t cb = dma_callback;
        void *d = dma_cbdata;

        dma_callback = NULL;
        dma_cbdata = 0;

        cb(d);
    }

    /* Signal the calling thread to continue, if any. */
    if(dma_blocking) {
        sem_signal(&dma_done);
        thd_schedule(1, 0);
        dma_blocking = 0;
    }
}

static uintptr_t pvr_dest_addr(uintptr_t dest, int type) {
    uintptr_t dest_addr;

    /* Send the data to the right place */
    if(type == PVR_DMA_TA) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_TA_INPUT;
    }
    else if(type == PVR_DMA_YUV) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_TA_YUV_CONV;
    }
    else if(type == PVR_DMA_VRAM64) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_TA_TEX_MEM;
    }
    else if(type == PVR_DMA_VRAM32) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_TA_TEX_MEM_32;
    }
    else if(type == PVR_DMA_VRAM64_SB) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_RAM_BASE_64_P0;
    }
    else if(type == PVR_DMA_VRAM32_SB) {
        dest_addr = ((uintptr_t)dest & 0xffffff) | PVR_RAM_BASE_32_P0;
    }
    else {
        dest_addr = dest;
    }

    return dest_addr;
}

int pvr_dma_transfer(void *src, uintptr_t dest, size_t count, int type,
                     int block, pvr_dma_callback_t callback, void *cbdata) {
    uintptr_t src_addr = ((uintptr_t)src);

    /* Check for 32-byte alignment */
    if(src_addr & 0x1F) {
        dbglog(DBG_ERROR, "pvr_dma: src is not 32-byte aligned\n");
        errno = EFAULT;
        return -1;
    }

    dma_blocking = block;
    dma_callback = callback;
    dma_cbdata = cbdata;

    /* Make sure we're not already DMA'ing */
    if(pvr_dma[PVR_DST] != 0) {
        dbglog(DBG_ERROR, "pvr_dma: Previous DMA has not finished\n");
        errno = EINPROGRESS;
        return -1;
    }

    if(DMAC_CHCR2 & 0x1)  /* DE bit set so we must clear it */
        DMAC_CHCR2 &= ~0x1;

    if(DMAC_CHCR2 & 0x2)  /* TE bit set so we must clear it */
        DMAC_CHCR2 &= ~0x2;

    DMAC_SAR2 = src_addr;
    DMAC_DMATCR2 = count / 32;
    DMAC_CHCR2 = 0x12c1;

    if((DMAC_DMAOR & DMAOR_STATUS_MASK) != DMAOR_NORMAL_OPERATION) {
        dbglog(DBG_ERROR, "pvr_dma: Failed DMAOR check\n");
        errno = EIO;
        return -1;
    }

    pvr_dma[PVR_STATE] = pvr_dest_addr(dest, type);
    pvr_dma[PVR_LEN] = count;
    pvr_dma[PVR_DST] = 0x1;

    /* Wait for us to be signaled */
    if(block)
        sem_wait(&dma_done);

    return 0;
}

/* Count is in bytes. */
int pvr_txr_load_dma(void *src, pvr_ptr_t dest, size_t count, int block,
                    pvr_dma_callback_t callback, void *cbdata) {
    return pvr_dma_transfer(src, (uintptr_t)dest, count, PVR_DMA_VRAM64, block, 
                            callback, cbdata);
}

int pvr_dma_load_ta(void *src, size_t count, int block, 
                    pvr_dma_callback_t callback, void *cbdata) {
    return pvr_dma_transfer(src, (uintptr_t)0, count, PVR_DMA_TA, block, callback, cbdata);
}

int pvr_dma_yuv_conv(void *src, size_t count, int block,
                    pvr_dma_callback_t callback, void *cbdata) {
    return pvr_dma_transfer(src, (uintptr_t)0, count, PVR_DMA_YUV, block, callback, cbdata);
}

int pvr_dma_ready(void) {
    return pvr_dma[PVR_DST] == 0;
}

void pvr_dma_init(void) {
    /* Create an initially blocked semaphore */
    sem_init(&dma_done, 0);
    dma_blocking = 0;
    dma_callback = NULL;
    dma_cbdata = 0;

    /* Use 2x32-bit TA->VRAM buses for PVR_TA_TEX_MEM */
    pvr_dma[PVR_LMMODE0] = 0;

    /* Use single 32-bit TA->VRAM bus for PVR_TA_TEX_MEM_32 */
    pvr_dma[PVR_LMMODE1] = 1;

    /* Hook the necessary interrupts */
    asic_evt_set_handler(ASIC_EVT_PVR_DMA, pvr_dma_irq_hnd, NULL);
    asic_evt_enable(ASIC_EVT_PVR_DMA, ASIC_IRQ_DEFAULT);
}

void pvr_dma_shutdown(void) {
    /* Need to ensure that no DMA is in progress */
    if(!pvr_dma_ready()) {
        pvr_dma[PVR_DST] = 0;
    }

    /* Clean up */
    asic_evt_disable(ASIC_EVT_PVR_DMA, ASIC_IRQ_DEFAULT);
    asic_evt_set_handler(ASIC_EVT_PVR_DMA, NULL, NULL);
    sem_destroy(&dma_done);
}

/* Copies n bytes from src to PVR dest, dest must be 32-byte aligned */
void *pvr_sq_load(void *dest, const void *src, size_t n, int type) {
    void *dma_area_ptr;

    if(pvr_dma[PVR_DST] != 0) {
        dbglog(DBG_ERROR, "pvr_sq_load: PVR DMA has not finished\n");
        errno = EINPROGRESS;
        return NULL;
    }

    dma_area_ptr = (void *)pvr_dest_addr((uintptr_t)dest, type);
    sq_cpy(dma_area_ptr, src, n);

    return dest;
}

/* Fills n bytes at PVR dest with 16-bit c, dest must be 32-byte aligned */
void *pvr_sq_set16(void *dest, uint32_t c, size_t n, int type) {
    void *dma_area_ptr;

    if(pvr_dma[PVR_DST] != 0) {
        dbglog(DBG_ERROR, "pvr_sq_set16: PVR DMA has not finished\n");
        errno = EINPROGRESS;
        return NULL;
    }

    dma_area_ptr = (void *)pvr_dest_addr((uintptr_t)dest, type);
    sq_set16(dma_area_ptr, c, n);

    return dest;
}

/* Fills n bytes at PVR dest with 32-bit c, dest must be 32-byte aligned */
void *pvr_sq_set32(void *dest, uint32_t c, size_t n, int type) {
    void *dma_area_ptr;

    if(pvr_dma[PVR_DST] != 0) {
        dbglog(DBG_ERROR, "pvr_sq_set32: PVR DMA has not finished\n");
        errno = EINPROGRESS;
        return NULL;
    }

    dma_area_ptr = (void *)pvr_dest_addr((uintptr_t)dest, type);
    sq_set32(dma_area_ptr, c, n);

    return dest;
}
