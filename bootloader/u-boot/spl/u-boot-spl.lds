MEMORY { .sram : ORIGIN = 0x60, LENGTH = 0x5fa0 }
MEMORY { .sdram : ORIGIN = 0x81f80000, LENGTH = 0x00080000 }
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
 .text :
 {
  __start = .;
  *(.vectors)
  arch/arm/cpu/arm926ejs/start.o (.text)
  *(.text*)
 } > .sram
 . = ALIGN(4);
 .rodata : { *(SORT_BY_ALIGNMENT(.rodata*)) } >.sram
 . = ALIGN(4);
 .data : { *(SORT_BY_ALIGNMENT(.data*)) } >.sram
 . = ALIGN(4);
 .u_boot_list : {
  KEEP(*(SORT(.u_boot_list*)));
 } > .sram
 . = ALIGN(4);
 __image_copy_end = .;
 _end = .;
 .bss :
 {
  . = ALIGN(4);
  __bss_start = .;
  *(.bss*)
  . = ALIGN(4);
  __bss_end = .;
 } > .sdram
}
