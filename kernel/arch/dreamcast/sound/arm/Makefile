MIKMOD_DIR := $(KOS_BASE)/kernel/arch/dreamcast/sound/arm/mikmod/libmikmod

MIKMOD_OBJS := $(MIKMOD_DIR)/loaders/load_669.o	\
               $(MIKMOD_DIR)/loaders/load_amf.o	\
               $(MIKMOD_DIR)/loaders/load_asy.o	\
               $(MIKMOD_DIR)/loaders/load_dsm.o	\
               $(MIKMOD_DIR)/loaders/load_far.o	\
               $(MIKMOD_DIR)/loaders/load_gdm.o	\
               $(MIKMOD_DIR)/loaders/load_gt2.o	\
               $(MIKMOD_DIR)/loaders/load_it.o	\
               $(MIKMOD_DIR)/loaders/load_imf.o	\
               $(MIKMOD_DIR)/loaders/load_m15.o	\
               $(MIKMOD_DIR)/loaders/load_med.o	\
               $(MIKMOD_DIR)/loaders/load_mod.o	\
               $(MIKMOD_DIR)/loaders/load_mtm.o	\
               $(MIKMOD_DIR)/loaders/load_okt.o	\
               $(MIKMOD_DIR)/loaders/load_s3m.o	\
               $(MIKMOD_DIR)/loaders/load_stm.o	\
               $(MIKMOD_DIR)/loaders/load_stx.o	\
               $(MIKMOD_DIR)/loaders/load_ult.o	\
               $(MIKMOD_DIR)/loaders/load_umx.o	\
               $(MIKMOD_DIR)/loaders/load_uni.o	\
               $(MIKMOD_DIR)/loaders/load_xm.o	\
               $(MIKMOD_DIR)/depackers/mmcmp.o	\
               $(MIKMOD_DIR)/depackers/pp20.o	\
               $(MIKMOD_DIR)/depackers/s404.o	\
               $(MIKMOD_DIR)/depackers/xpk.o	\
               $(MIKMOD_DIR)/mmio/mmalloc.o	\
               $(MIKMOD_DIR)/mmio/mmerror.o	\
               $(MIKMOD_DIR)/mmio/mmio.o		\
               $(MIKMOD_DIR)/posix/strcasecmp.o	\
               $(MIKMOD_DIR)/playercode/mdreg.o	\
               $(MIKMOD_DIR)/playercode/mdriver.o	\
               $(MIKMOD_DIR)/playercode/mloader.o	\
               $(MIKMOD_DIR)/playercode/mlreg.o	\
               $(MIKMOD_DIR)/playercode/mlutil.o	\
               $(MIKMOD_DIR)/playercode/mplayer.o	\
               $(MIKMOD_DIR)/playercode/munitrk.o	\
               $(MIKMOD_DIR)/playercode/mwav.o	\
               $(MIKMOD_DIR)/playercode/npertab.o	\
               $(MIKMOD_DIR)/playercode/sloader.o

TARGET = stream.drv
OBJS := $(MIKMOD_OBJS) main.o player.o

.PHONY: all clean aicaos

CC = $(DC_ARM_CC)
AS = $(DC_ARM_AS)
CPPFLAGS = $(DC_ARM_INCS) -I $(KOS_BASE)/kernel/arch/dreamcast/include/dc/sound
CFLAGS = $(DC_ARM_CFLAGS)
ASFLAGS = $(DC_ARM_AFLAGS)
LDFLAGS = $(DC_ARM_LDFLAGS) $(DC_ARM_LIB_PATHS) -Wl,-Map,prog.map
LDLIBS = $(DC_ARM_LIBS)

all: $(TARGET)

aicaos:
	$(MAKE) -C aicaos

ARM_CC_IS_AVAILABLE=0
ifdef DC_ARM_CC
  ifneq ("$(wildcard $(DC_ARM_CC))", "") 
    ARM_CC_IS_AVAILABLE=1
  endif
endif

ifeq ($(ARM_CC_IS_AVAILABLE), 1)
# Only compile this if we have an ARM compiler handy
stream.drv: prog.elf
	$(DC_ARM_OBJCOPY) -O binary $< $@
else
# Otherwise use precompiled ARM binary
stream.drv: stream.drv.prebuilt
	cp $< $@
endif

prog.elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJS): aicaos

$(MIKMOD_OBJS) player.o: DC_ARM_CFLAGS += -I$(MIKMOD_DIR)/include -DHAVE_LIMITS_H -DHAVE_POSIX_MEMALIGN

clean-aicaos:
	$(MAKE) -C aicaos clean

clean: clean-aicaos
	-rm -f $(OBJS) prog.elf prog.map $(TARGET)
