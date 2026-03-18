#
# lftpd
#

KOS_BUILD_SUBARCHS = pristine

TARGET = lftpd.elf
OBJS = lftpd.o lftpd_inet.o lftpd_io.o lftpd_log.o lftpd_string.o

KOS_CFLAGS += -Iinclude -I.

all: $(TARGET)

include $(KOS_BASE)/Makefile.rules

$(TARGET): $(OBJS)
	kos-cc -o $@ $^ -lkosfat

clean:
	-rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)
