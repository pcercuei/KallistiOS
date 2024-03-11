TARGET = libaicaos.a
OBJS = aica.o assert.o crt0.o irq.o lock.o main.o mm.o printf.o queue.o task.o task_asm.o

CC = $(DC_ARM_CC)
AS = $(DC_ARM_AS)
CPPFLAGS = $(DC_ARM_INCS) -I $(KOS_BASE)/kernel/arch/dreamcast/include/dc/sound
CFLAGS = $(DC_ARM_CFLAGS) -ffreestanding
ASFLAGS = $(DC_ARM_AFLAGS)
LDFLAGS = $(DC_ARM_LDFLAGS)

.PHONY: all clean

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(DC_ARM_AR) rcs $@ $^
