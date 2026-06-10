/* KallistiOS ##version##

   dc/aica_registers.h
   Copyright (C) 2026 Paul Cercueil

   Definitions for the AICA registers.
   This file can be included from both the ARM and SH-4 sides.
*/

#ifndef __AICA_REGISTERS_H
#define __AICA_REGISTERS_H

#include <kos/regfield.h>

#define AICA_REGISTERS_BASE_SH4 0xa0700000
#define AICA_REGISTERS_BASE_ARM 0x00800000

#ifdef __sh__
#define AICA_REGISTERS_BASE     AICA_REGISTERS_BASE_SH4
#else
#define AICA_REGISTERS_BASE     AICA_REGISTERS_BASE_ARM
#endif

#define SPU_REG32(reg)          *(volatile unsigned int *)(reg)

#define SPU_REG(reg)            (AICA_REGISTERS_BASE + (reg))
#define CHN_REG(chn, reg)       SPU_REG(0x80 * (chn) + (reg))

#define REG_SPU_PLAY_CTRL(chn)  CHN_REG((chn), 0x00)
#define REG_SPU_ADDR_L(chn)     CHN_REG((chn), 0x04)
#define REG_SPU_LOOP_START(chn) CHN_REG((chn), 0x08)
#define REG_SPU_LOOP_END(chn)   CHN_REG((chn), 0x0c)
#define REG_SPU_AMP_ENV1(chn)   CHN_REG((chn), 0x10)
#define REG_SPU_AMP_ENV2(chn)   CHN_REG((chn), 0x14)
#define REG_SPU_PITCH(chn)      CHN_REG((chn), 0x18)
#define REG_SPU_LFO(chn)        CHN_REG((chn), 0x1c)
#define REG_SPU_DSP(chn)        CHN_REG((chn), 0x20)
#define REG_SPU_VOL_PAN(chn)    CHN_REG((chn), 0x24)
#define REG_SPU_LPF1(chn)       CHN_REG((chn), 0x28)
#define REG_SPU_LPF2(chn)       CHN_REG((chn), 0x2c)
#define REG_SPU_LPF3(chn)       CHN_REG((chn), 0x30)
#define REG_SPU_LPF4(chn)       CHN_REG((chn), 0x34)
#define REG_SPU_LPF5(chn)       CHN_REG((chn), 0x38)
#define REG_SPU_LPF6(chn)       CHN_REG((chn), 0x3c)
#define REG_SPU_LPF7(chn)       CHN_REG((chn), 0x40)
#define REG_SPU_LPF8(chn)       CHN_REG((chn), 0x44)

#define REG_SPU_DSP_MIXER(chn)  SPU_REG(0x2000 + 0x4 * (chn))

#define REG_SPU_CDDA_LEFT       SPU_REG(0x2040)
#define REG_SPU_CDDA_RIGHT      SPU_REG(0x2044)

#define REG_SPU_MASTER_VOL      SPU_REG(0x2800)
#define REG_SPU_DSP_RB_ADDR     SPU_REG(0x2804)
#define REG_SPU_BUS_REQUEST     SPU_REG(0x2808)
#define REG_SPU_INFO_REQUEST    SPU_REG(0x280c)
#define REG_SPU_INFO_PLAY_POS   SPU_REG(0x2814)
#define REG_SPU_TIMER0_CTRL     SPU_REG(0x2890)
#define REG_SPU_TIMER1_CTRL     SPU_REG(0x2894)
#define REG_SPU_TIMER2_CTRL     SPU_REG(0x2898)
#define REG_SPU_INT_ENABLE      SPU_REG(0x289c)
#define REG_SPU_INT_SEND        SPU_REG(0x28a0)
#define REG_SPU_INT_RESET       SPU_REG(0x28a4)
#define REG_SPU_FIQ_BIT_0       SPU_REG(0x28a8)
#define REG_SPU_FIQ_BIT_1       SPU_REG(0x28ac)
#define REG_SPU_FIQ_BIT_2       SPU_REG(0x28b0)
#define REG_SPU_SH4_INT_ENABLE  SPU_REG(0x28b4)
#define REG_SPU_SH4_INT_SEND    SPU_REG(0x28b8)
#define REG_SPU_SH4_INT_RESET   SPU_REG(0x28bc)
#define REG_SPU_ARM_CTRL        SPU_REG(0x2c00)
#define REG_SPU_INT_REQUEST     SPU_REG(0x2d00)
#define REG_SPU_INT_CLEAR       SPU_REG(0x2d04)

/* Register fields below */

#define SPU_PLAY_CTRL_KEY       GENMASK(15, 14)
#define SPU_PLAY_CTRL_LOOP      BIT(9)
#define SPU_PLAY_CTRL_FORMAT    GENMASK(8, 7)
#define SPU_PLAY_CTRL_ADDR_H    GENMASK(6, 0)

#define SPU_AMP_ENV1_DECAY2     GENMASK(15, 11)
#define SPU_AMP_ENV1_DECAY1     GENMASK(10, 6)
#define SPU_AMP_ENV1_ATTACK     GENMASK(4, 0)

#define SPU_AMP_ENV2_LINK       BIT(14)
#define SPU_AMP_ENV2_KEY        GENMASK(13, 10)
#define SPU_AMP_ENV2_DECAY_LVL  GENMASK(9, 5)
#define SPU_AMP_ENV2_RELEASE    GENMASK(4, 0)

#define SPU_PITCH_OCT           GENMASK(15, 11)
#define SPU_PITCH_FNS           GENMASK(9, 0)

#define SPU_LFO_RESET           BIT(15)
#define SPU_LFO_FREQ            GENMASK(14, 10)
#define SPU_LFO_FORM1           GENMASK(9, 8)
#define SPU_LFO_DEPTH1          GENMASK(7, 5)
#define SPU_LFO_FORM2           GENMASK(4, 3)
#define SPU_LFO_DEPTH2          GENMASK(2, 0)

#define SPU_DSP_SEND            GENMASK(7, 4)
#define SPU_DSP_CHN             GENMASK(3, 0)

#define SPU_VOL_PAN_VOL         GENMASK(11, 8)
#define SPU_VOL_PAN_PAN         GENMASK(4, 0)

#define SPU_LPF1_VOL            GENMASK(15, 8)
#define SPU_LPF1_OFF            BIT(5)
#define SPU_LPF1_Q              GENMASK(4, 0)

#define SPU_LPF2_VAL            GENMASK(12, 0)

#define SPU_LPF3_VAL            GENMASK(12, 0)

#define SPU_LPF4_VAL            GENMASK(12, 0)

#define SPU_LPF5_VAL            GENMASK(12, 0)

#define SPU_LPF6_VAL            GENMASK(12, 0)

#define SPU_LPF7_ATTACK         GENMASK(12, 8)
#define SPU_LPF7_DECAY          GENMASK(7, 0)

#define SPU_LPF8_DECAY          GENMASK(12, 8)
#define SPU_LPF8_RELEASE        GENMASK(7, 0)

#define SPU_DSP_MIXER_VOL       GENMASK(11, 8)
#define SPU_DSP_MIXER_PAN       GENMASK(4, 0)

#define SPU_CDDA_VOL            GENMASK(11, 8)
#define SPU_CDDA_PAN            GENMASK(4, 0)

#define SPU_MASTER_VOL_MONO     BIT(15)
#define SPU_MASTER_VOL_8MB      BIT(9)
#define SPU_MASTER_VOL_VOL      GENMASK(3, 0)

#define SPU_DSP_RB_ADDR_SIZE    GENMASK(14, 13)
#define SPU_DSP_RB_ADDR_PTR     GENMASK(11, 0)

#define SPU_ARM_CTRL_RESET      BIT(1)

#define SPU_INFO_REQUEST_REQ    GENMASK(13, 8)

#define SPU_INT_REQUEST_CODE    GENMASK(2, 0)

enum spu_timer_ctrl_div {
    SPU_TIMER_CTRL_DIV_1,
    SPU_TIMER_CTRL_DIV_2,
    SPU_TIMER_CTRL_DIV_4,
    SPU_TIMER_CTRL_DIV_8,
    SPU_TIMER_CTRL_DIV_16,
    SPU_TIMER_CTRL_DIV_32,
    SPU_TIMER_CTRL_DIV_64,
    SPU_TIMER_CTRL_DIV_128,
};

#define SPU_TIMER_CTRL_START    GENMASK(7, 0)
#define SPU_TIMER_CTRL_DIV      GENMASK(10, 8)

enum spu_int_codes {
    SPU_INT_TIMER               = 2,
    SPU_INT_SH4                 = 4,
    SPU_INT_BUS                 = 5,
};

/* These bits are used in REG_SPU_INT_ENABLE and other registers. */
#define SPU_INT_ENABLE_SH4      BIT(5)
#define SPU_INT_ENABLE_TIMER0   BIT(6)
#define SPU_INT_ENABLE_TIMER1   BIT(7)
#define SPU_INT_ENABLE_TIMER2   BIT(8)
#define SPU_INT_ENABLE_BUS      BIT(8)

#endif /* __AICA_REGISTERS_H */
