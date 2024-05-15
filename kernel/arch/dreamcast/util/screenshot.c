/* KallistiOS ##version##

   screenshot.c

   Copyright (C) 2002 Megan Potter
   Copyright (C) 2008 Donald Haase
   Copyright (C) 2024 Andy Barajas

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dc/video.h>
#include <kos/fs.h>
#include <arch/irq.h>

/*

Provides a very simple screen shot facility (dumps raw RGB PPM files from the
currently viewed framebuffer).

Destination file system must be writeable and have enough free space.

This will now work with any of the supported video modes.

*/

static inline uint32_t bswap8(uint32_t val) {
    return (val & 0xffff0000) | ((val & 0xff00) >> 8) | ((val & 0xff) << 8);
}

int vid_screen_shot(const char *destfn) {
    file_t   f;
    uint8_t  *buffer;
    uint8_t  *vram_b; /* Used for PM_RGB888P(24-bit) */
    char     header[256];
    int      i, numpix;
    uint32_t save;
    uint32_t pixel, pixels21, pixels43;
    uint8_t  r, g, b;
    uint8_t  bpp;
    uint32_t *buffer32, *vram32, r1, r2, g1, g2, b1, b2;
    uint32_t *vram = vram_l;

    bpp = 3;    /* output to ppm is 3 bytes per pixel */
    numpix = vid_mode->width * vid_mode->height;

    /* Allocate a new buffer so we can blast it all at once */
    buffer = (uint8_t *)malloc(numpix * bpp);

    if(buffer == NULL) {
        dbglog(DBG_ERROR, "vid_screen_shot: can't allocate ss memory\n");
        return -1;
    }

    buffer32 = (uint32_t *)buffer;

    /* Open output file */
    f = fs_open(destfn, O_WRONLY | O_TRUNC);

    if(!f) {
        dbglog(DBG_ERROR, "vid_screen_shot: can't open output file '%s'\n", destfn);
        free(buffer);
        return -1;
    }

    /* Disable interrupts */
    save = irq_disable();

    /* Write out each pixel as 24-bits */
    switch(vid_mode->pm) {
        case(PM_RGB555): { /* (15-bit) */
            /* Process four 16-bit pixels at a time */
            for(i = 0; i < numpix/4; i++) {
                pixels21 = *vram++;
                pixels43 = *vram++;

                r1 = bswap8((pixels21 << 1) & 0xf800f800); /* r1: 0xf80000f8 */
                r2 = bswap8((pixels43 << 1) & 0xf800f800); /* r2: 0xf80000f8 */

                g1 = bswap8((pixels21 << 6) & 0xf800f800); /* g1: 0xf80000f8 */
                g2 = bswap8((pixels43 << 6) & 0xf800f800); /* g2: 0xf80000f8 */

                b1 = bswap8((pixels21 << 11) & 0xf800f800); /* b1: 0xf80000f8 */
                b2 = bswap8((pixels43 << 11) & 0xf800f800); /* b2: 0xf80000f8 */

                /* Write RBGR */
                *buffer32++ = r1 | (g1 << 8) | (b1 << 16);

                /* Write GRBG */
                *buffer32++ = (g1 >> 24) | (r2 << 16) | (b1 >> 16) | (g2 << 24);

                /* Write BGRB */
                *buffer32++ = b2 | (r2 >> 16) | (g2 >> 8);
            }
            break;
        }
        case(PM_RGB565): { /* (16-bit) */
            /* Process four 16-bit pixels at a time */
            for(i = 0; i < numpix/4; i++) {
                pixels21 = *vram++;
                pixels43 = *vram++;

                r1 = bswap8(pixels21 & 0xf800f800); /* r1: 0xf80000f8 */
                r2 = bswap8(pixels43 & 0xf800f800); /* r2: 0xf80000f8 */

                g1 = bswap8((pixels21 << 5) & 0xfc00fc00); /* g1: 0xfc0000fc */
                g2 = bswap8((pixels43 << 5) & 0xfc00fc00); /* g2: 0xfc0000fc */

                b1 = bswap8((pixels21 << 11) & 0xf800f800); /* b1: 0xf80000f8 */
                b2 = bswap8((pixels43 << 11) & 0xf800f800); /* b2: 0xf80000f8 */

                /* Write RBGR */
                *buffer32++ = r1 | (g1 << 8) | (b1 << 16);

                /* Write GRBG */
                *buffer32++ = (g1 >> 24) | (r2 << 16) | (b1 >> 16) | (g2 << 24);

                /* Write BGRB */
                *buffer32++ = b2 | (r2 >> 16) | (g2 >> 8);
            }
            break;
        }
        case(PM_RGB888P): { /* (24-bit) */
            vram_b = (uint8_t *)vram_l;
            for(i = 0; i < numpix; i++) {
                buffer[i * 3 + 0] = vram_b[i * 3 + 2];
                buffer[i * 3 + 1] = vram_b[i * 3 + 1];
                buffer[i * 3 + 2] = vram_b[i * 3 + 0];
            }

            break;
        }
        case(PM_RGB0888): { /* (32-bit) */
            for(i = 0; i < numpix; i++) {
                pixel = vram_l[i];
                r = (((pixel >> 16) & 0xff));
                g = (((pixel >>  8) & 0xff));
                b = (((pixel >>  0) & 0xff));
                buffer[i * 3 + 0] = r;
                buffer[i * 3 + 1] = g;
                buffer[i * 3 + 2] = b;
            }

            break;
        }

        default: {
            dbglog(DBG_ERROR, "vid_screen_shot: can't process pixel mode %d\n", vid_mode->pm);
            irq_restore(save);
            fs_close(f);
            free(buffer);
            return -1;
        }
    }

    irq_restore(save);

    /* Write a small header */
    sprintf(header, "P6\n#KallistiOS Screen Shot\n%d %d\n255\n", vid_mode->width, vid_mode->height);

    if(fs_write(f, header, strlen(header)) != (ssize_t)strlen(header)) {
        dbglog(DBG_ERROR, "vid_screen_shot: can't write header to output file '%s'\n", destfn);
        fs_close(f);
        free(buffer);
        return -1;
    }

    /* Write the data */
    if(fs_write(f, buffer, numpix * bpp) != (ssize_t)(numpix * bpp)) {
        dbglog(DBG_ERROR, "vid_screen_shot: can't write data to output file '%s'\n", destfn);
        fs_close(f);
        free(buffer);
        return -1;
    }

    fs_close(f);
    free(buffer);

    dbglog(DBG_INFO, "vid_screen_shot: written to output file '%s'\n", destfn);

    return 0;
}
