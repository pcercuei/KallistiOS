/* KallistiOS ##version##

   controller.c
   Copyright (C) 2002 Megan Potter

 */

#include <arch/arch.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <string.h>
#include <assert.h>

/* Location of controller capabilities within function_data array */
#define CONT_FUNCTION_DATA_INDEX  0 

/* Raw controller condition structure */
typedef struct cont_cond {
    uint16_t buttons;  /* buttons bitfield */
    uint8_t rtrig;     /* right trigger */
    uint8_t ltrig;     /* left trigger */
    uint8_t joyx;      /* joystick X */
    uint8_t joyy;      /* joystick Y */
    uint8_t joy2x;     /* second joystick X */
    uint8_t joy2y;     /* second joystick Y */
} cont_cond_t;

static cont_btn_callback_t btn_callback = NULL;
static uint8_t btn_callback_addr = 0;
static uint32_t btn_callback_btns = 0;

/* Check whether the controller has EXACTLY the given capabilities. */
int cont_is_type(const maple_device_t *cont, uint32_t type) {
    return cont ? cont->info.function_data[CONT_FUNCTION_DATA_INDEX] == type :
                  -1;
}

/* Check whether the controller has at LEAST the given capabilities. */
int cont_has_capabilities(const maple_device_t *cont, uint32_t capabilities) {
    return cont ? ((cont->info.function_data[CONT_FUNCTION_DATA_INDEX] 
                   & capabilities) == capabilities) : -1;
}

/* Set a controller callback for a button combo; set addr=0 for any controller */
void cont_btn_callback(uint8_t addr, uint32_t btns, cont_btn_callback_t cb) {
    btn_callback_addr = addr;
    btn_callback_btns = btns;
    btn_callback = cb;
}

/* Response callback for the GETCOND Maple command. */
static void cont_reply(maple_state_t *, maple_frame_t *frm) {
    maple_response_t *resp;
    uint32_t         *respbuf;
    cont_cond_t      *raw;
    cont_state_t     *cooked;

    /* Unlock the frame now (it's ok, we're in an IRQ) */
    maple_frame_unlock(frm);

    /* Make sure we got a valid response */
    resp = (maple_response_t *)frm->recv_buf;

    if(resp->response != MAPLE_RESPONSE_DATATRF)
        return;

    respbuf = (uint32_t *)resp->data;

    if(respbuf[0] != MAPLE_FUNC_CONTROLLER)
        return;

    /* Update the status area from the response */
    if(frm->dev) {
        /* Verify the size of the frame and grab a pointer to it */
        assert(sizeof(cont_cond_t) == ((resp->data_len - 1) * sizeof(uint32_t)));
        raw = (cont_cond_t *)(respbuf + 1);

        /* Fill the "nice" struct from the raw data */
        cooked = (cont_state_t *)(frm->dev->status);
        cooked->buttons = (~raw->buttons) & 0xffff;
        cooked->ltrig = raw->ltrig;
        cooked->rtrig = raw->rtrig;
        cooked->joyx = ((int)raw->joyx) - 128;
        cooked->joyy = ((int)raw->joyy) - 128;
        cooked->joy2x = ((int)raw->joy2x) - 128;
        cooked->joy2y = ((int)raw->joy2y) - 128;
        frm->dev->status_valid = 1;

        /* Check for magic button sequences */
        if(btn_callback) {
            if(!btn_callback_addr ||
                    (btn_callback_addr &&
                     btn_callback_addr == maple_addr(frm->dev->port, frm->dev->unit))) {
                if((cooked->buttons & btn_callback_btns) == btn_callback_btns) {
                    btn_callback(maple_addr(frm->dev->port, frm->dev->unit),
                                 cooked->buttons);
                }
            }
        }
    }
}

static int cont_poll(maple_device_t *dev) {
    uint32_t *send_buf;

    if(maple_frame_lock(&dev->frame) < 0)
        return 0;

    maple_frame_init(&dev->frame);
    send_buf = (uint32_t *)dev->frame.recv_buf;
    send_buf[0] = MAPLE_FUNC_CONTROLLER;
    dev->frame.cmd = MAPLE_COMMAND_GETCOND;
    dev->frame.dst_port = dev->port;
    dev->frame.dst_unit = dev->unit;
    dev->frame.length = 1;
    dev->frame.callback = cont_reply;
    dev->frame.send_buf = send_buf;
    maple_queue_frame(&dev->frame);

    return 0;
}

static void cont_periodic(maple_driver_t *drv) {
    maple_driver_foreach(drv, cont_poll);
}

/* Device Driver Struct */
static maple_driver_t controller_drv = {
    .functions = MAPLE_FUNC_CONTROLLER,
    .name = "Controller Driver",
    .periodic = cont_periodic,
    .attach = NULL,
    .detach = NULL
};

/* Add the controller to the driver chain */
void cont_init(void) {
    if(!controller_drv.drv_list.le_prev)
        maple_driver_reg(&controller_drv);
}

void cont_shutdown(void) {
    maple_driver_unreg(&controller_drv);
}
