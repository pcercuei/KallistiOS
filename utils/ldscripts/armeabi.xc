/* Script for -z combreloc: combine and sort reloc sections */

OUTPUT_ARCH(arm)
STARTUP(_kos_startup.o)
INPUT(crti.o)
INPUT(crtbegin.o)
INPUT(crtend.o)
INPUT(crtn.o)
LOAD_OFFSET = DEFINED(LOAD_OFFSET) ? LOAD_OFFSET : 0x0 ;

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  PROVIDE (__executable_start = LOAD_OFFSET); . = LOAD_OFFSET;

  INCLUDE common.xc

  __stack = 0x1FE000;
  __fiq_stack = 0x1FFFF0;
}
