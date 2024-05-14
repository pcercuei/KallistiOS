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

int vid_screen_shot(const char *destfn) {
    file_t   f;
    uint8_t  *buffer;
    char     header[256];
    int      i, numpix;
    uint32_t save;
    uint32_t pixel, pixel1, pixel2;  /* to fit 888 mode */
    uint8_t  r, g, b;
    uint8_t  bpp;
    uint8_t  *vram_b = (uint8_t *)vram_l;

    bpp = 3;    /* output to ppm is 3 bytes per pixel */
    numpix = vid_mode->width * vid_mode->height;

    /* Allocate a new buffer so we can blast it all at once */
    buffer = (uint8_t *)malloc(numpix * bpp);

    if(buffer == NULL) {
        dbglog(DBG_ERROR, "vid_screen_shot: can't allocate ss memory\n");
        return -1;
    }

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
            /* Process two 16-bit pixels at a time */
            for(i = 0; i < numpix/2; i++) {
                pixel = vram_l[i];
                pixel1 = pixel & 0xFFFF;
                pixel2 = pixel >> 16;

                /* Process the first pixel */
                r = (((pixel1 >> 10) & 0x1f) << 3);
                g = (((pixel1 >> 5) & 0x1f) << 3);
                b = (((pixel1 >> 0) & 0x1f) << 3);
                buffer[i * 6 + 0] = r;
                buffer[i * 6 + 1] = g;
                buffer[i * 6 + 2] = b;

                /* Process the second pixel */
                r = (((pixel2 >> 10) & 0x1f) << 3);
                g = (((pixel2 >> 5) & 0x1f) << 3);
                b = (((pixel2 >> 0) & 0x1f) << 3);
                buffer[i * 6 + 3] = r;
                buffer[i * 6 + 4] = g;
                buffer[i * 6 + 5] = b;
            }

            break;
        }
        case(PM_RGB565): { /* (16-bit) */
            /* Process two 16-bit pixels at a time */
            for(i = 0; i < numpix/2; i++) {
                pixel = vram_l[i];
                pixel1 = pixel & 0xFFFF;
                pixel2 = pixel >> 16;

                /* Process the first pixel */
                r = (((pixel1 >> 11) & 0x1f) << 3);
                g = (((pixel1 >> 5) & 0x3f) << 2);
                b = (((pixel1 >> 0) & 0x1f) << 3);
                buffer[i * 6 + 0] = r;
                buffer[i * 6 + 1] = g;
                buffer[i * 6 + 2] = b;

                /* Process the second pixel */
                r = (((pixel2 >> 11) & 0x1f) << 3);
                g = (((pixel2 >> 5) & 0x3f) << 2);
                b = (((pixel2 >> 0) & 0x1f) << 3);
                buffer[i * 6 + 3] = r;
                buffer[i * 6 + 4] = g;
                buffer[i * 6 + 5] = b;
            }

            break;
        }
        case(PM_RGB888P): { /* (24-bit) */
            for(i = 0; i < numpix; i++) {
                buffer[0] = vram_b[i * 3 + 2];
				buffer[1] = vram_b[i * 3 + 1];
				buffer[2] = vram_b[i * 3];
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
