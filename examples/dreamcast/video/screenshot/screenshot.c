/* KallistiOS ##version##

   screenshot.c
   Copyright (C) 2024 Andy Barajas

*/

/* 
   This program demonstrates how to use the vid_screen_shot() function
   to capture and save a screenshot in the PPM format to your computer
   using the DC Tool. This tool requires the '-c "."' command-line argument
   to operate correctly.

   The program cycles through a color gradient background and allows user
   interaction to capture screenshots or exit the program.

   Usage:
   Ensure the '/pc/' directory path is correctly specified in the vid_screen_shot()
   function call so that the screenshot.ppm file is saved in the appropriate
   directory on your computer.
*/ 

#include <dc/video.h>
#include <dc/fmath.h>
#include <dc/maple.h>
#include <dc/biosfont.h>
#include <dc/maple/controller.h>

#include <kos/thread.h>

int main(int argc, char **argv) {
    uint8_t r, g, b;
    uint32_t t = 0;
    int font_height_offset = 0;

    /* Adjust frequency for faster or slower transitions */
    float frequency = 0.01; 
    
    maple_device_t *cont;
    cont_state_t *state;

    /* Set the video mode */
    vid_set_mode(DM_640x480, PM_RGB565);   

    while(1) {
        if((cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER)) != NULL) {
            state = (cont_state_t *)maple_dev_status(cont);

            if(state == NULL)
                break;

            if(state->buttons & CONT_START)
                break;

            if(state->buttons & CONT_A)
                vid_screen_shot("/pc/screenshot.ppm");
        }

        /* Wait for VBlank */
        vid_waitvbl();
        
        /* Calculate next background color */
        r = (uint8_t)((fsin(frequency * t + 0) * 127.5) + 127.5);
        g = (uint8_t)((fsin(frequency * t + 2 * F_PI / 3) * 127.5) + 127.5);
        b = (uint8_t)((fsin(frequency * t + 4 * F_PI / 3) * 127.5) + 127.5);

        t += 1; /* Increment t to change color in the next cycle */
        if(t == INT32_MAX) t = 0; /* Reset t to avoid overflow and ensure smooth cycling */

        /* Draw Background */
        vid_clear(r, g, b);

        /* Draw Foreground */
        font_height_offset = (640 * (480 - (BFONT_HEIGHT * 6))) + (BFONT_THIN_WIDTH * 2);
        bfont_draw_str(vram_s + font_height_offset, 640, 1, "Press Start to exit");
        font_height_offset += 640 * BFONT_HEIGHT * 2;
        bfont_draw_str(vram_s + font_height_offset, 640, 1, "Press A to take a screen shot");

        /* Without this the bfont wont show on the screen */
        thd_sleep(10);
    }

    return 0;
}
