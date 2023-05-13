# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

.set KERNEL_VIRTUAL_BASE, 0xC0000000                  # 3GB
.set KERNEL_PAGE_NUMBER, (KERNEL_VIRTUAL_BASE >> 22)  # Page directory index of kernel's 4MB PTE. 768
.set KERNEL_STACK_SIZE, 0x4000 												# reserve initial kernel stack space -- that's 16k.

# Declare a header as in the Multiboot Standard.
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
hello_world:    .asciz "HELLO MY FIRENDS \n"

.section .data
.align 0x1000 // 4KB
boot_page_directory:
.long 0x00000083
.fill (KERNEL_PAGE_NUMBER - 1), 4, 0                 # Pages before kernel space.
# This page directory entry defines a 4MB page containing the kernel.
.long 0x00000083
.fill (1024 - KERNEL_PAGE_NUMBER - 1), 4, 0  # Pages after the kernel image.

# Reserve a stack for the initial thread.
.section .bss
.section .stack
.align 16
stack_bottom:
.skip KERNEL_STACK_SIZE # 16 KiB
stack_top:

# The kernel entry point.
.section .text
.global _start
.type _start, @function
_start:


kernel_enable_paging:
	# NOTE: Until paging is set up, the code must be position-independent and use physical
	# addresses, not virtual ones!
	//mov [boot_page_directory], %ecx
  movl $boot_page_directory, %ecx
  sub $KERNEL_VIRTUAL_BASE, %ecx
	mov %ecx, %cr3                                        # Load Page Directory Base Register.

	mov %cr4, %ecx 
	or $0x00000010, %ecx                           # Set PSE bit in CR4 to enable 4MB pages.
	mov %ecx, %cr4

	mov %cr0, %ecx
	or $0x80000000, %ecx                           # Set PG bit in CR0 to enable paging.
	mov %ecx, %cr0 

	# Start fetching instructions in kernel space.
	# Since eip at this point holds the physical address of this command (approximately 0x00100000)
	# we need to do a long jump to the correct virtual address of StartInHigherHalf which is
	# approximately 0xC0100000.
	lea [start_in_higher_half], %ecx
	jmp %ecx                                                     # NOTE: Must be absolute jump!

start_in_higher_half:
	# Unmap the identity-mapped first 4MB of physical address space. It should not be needed
	# anymore.
	movl $0, [boot_page_directory]
	invlpg [0]

	# NOTE: From now on, paging should be enabled. The first 4MB of physical address space is
	# mapped smovl $TEST, %ebx tarting at KERNEL_VIRTUAL_BASE. Everything is linked to this address, so no more
	# position-independent code or funny business with virtual-to-physical address translation
	# should be necessary. We now have a higher-half kernel.

  movl $stack_top, %esp

	# Call the global constructors.
  # pushing Multiboot v1 params onto the stack
  push %eax 
  add $KERNEL_VIRTUAL_BASE, %ebx  # make the address virtual
  push %ebx
  sub $0x8, %esp
  call _init

  # Init GDB and IDT tables and move to the protected mode
  add $0x8, %esp

  # Transfer control to the main kernel
	call kernel_main
	# mov kernel_stack + KERNEL_STACK_SIZE, %esp           # set up the stack

	# push eax                           ; pass Multiboot magic number

	# pass Multiboot info structure -- WARNING: This is a physical address and may not be
	# in the first 4MB!
	
1:	hlt
	jmp 1b
.size _start, . - _start
.end .




