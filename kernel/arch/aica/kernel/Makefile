COPYOBJS = arch_exports.o debug.o dsp.o vfs.o init.o irq.o \
	   panic.o queue.o stack.o subarch_exports.o thdswitch.o timer.o \
	   tls_static.o puts.o
OBJS = $(COPYOBJS) startup.o

myall: $(OBJS)
	-cp $(COPYOBJS) $(KOS_BASE)/kernel/build/
	-cp startup.o $(KOS_BASE)/lib/$(KOS_ARCH)/_kos_startup.o

include $(KOS_BASE)/Makefile.prefab

arch_exports.c: ../exports.txt
	$(KOS_BASE)/utils/genexports/genexports.sh $< $@ arch_symtab

subarch_exports.c: ../exports-$(KOS_SUBARCH).txt
	$(KOS_BASE)/utils/genexports/genexports.sh $< $@ subarch_symtab

clean:
	-rm -f $(OBJS) arch_exports.c subarch_exports.c
