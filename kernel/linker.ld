/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
  kernel_boot = 0x00100000;
	kernel_higher_half = 0xC0000000;
  stack_size_bytes = 0x4000; /* 16384 */
  
	. = kernel_higher_half + kernel_boot;
  kernel_start = .;
  
	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.text ALIGN(4096) : AT(ADDR(.text) - kernel_higher_half)
	{
    kernel_text_start = .;
		*(.multiboot)
		*(.text .text.*)
    kernel_text_end = .;
	}


	/* Read-only data. */
	.rodata ALIGN(4096) : AT(ADDR(.rodata) - kernel_higher_half)
	{
		*(.rodata .rodata.*)
	}
 
	/* Read-write data (initialized) */
	.data ALIGN(4096) : AT(ADDR(.data) - kernel_higher_half)
	{
		kernel_data_start = .;
		*(.data .data.*)
		*(.symbols)
		kernel_data_end = .;
	}
 
	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4096) : AT(ADDR(.bss) - kernel_higher_half)
	{
		*(COMMON)
		*(.bss .bss.*)
    stack_bottom = .;
		*(.stack)
    stack_top = stack_bottom + stack_size_bytes;
	}

  kernel_end = .;

	/* The compiler may produce other sections, put them in the proper place in
	   in this file, if you'd like to include them in the final kernel. */
}