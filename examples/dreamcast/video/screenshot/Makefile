# KallistiOS ##version##
#
# video/screenshot/Makefile
# Copyright (C) 2024 Andy Barajas
#

TARGET = screenshot.elf
OBJS = screenshot.o

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -f test.ppm

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS) 
	kos-cc -o $(TARGET) $(OBJS)

# For Mac & Linux, sudo is required with the -c "." command to save data to the PC
run: $(TARGET)
	#sudo $(KOS_LOADER) $(TARGET) -c "."
	$(KOS_LOADER) $(TARGET) -c "."

dist: $(TARGET)
	-rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)
