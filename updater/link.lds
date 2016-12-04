ENTRY(main)

SECTIONS
{
  . = 0x20001400;
  .text :
  {
   main = .;
   *(.text*)
   *(.text)
   *(.rdata)
   *(.rodata)
   *(.rodata*)
   . = ALIGN(4);
  }

  .data :
  {
   *(.data)
   *(.data*)
   *(.idata)
   . = ALIGN(4);
  }
  bss = .;
  .bss :
  {
   *(.bss)
   *(.bss*)
  }
  end = . ;
  /DISCARD/ :
  {
  *(.note)
  *(.note*)
  *(.comment)
  }
}

