hello_world:    .asciz "HELLO MY FIRENDS \n"

.global enter_usermode
.type enter_usermode, @function
enter_usermode:
	mov $0x23, %ax	        # user mode data selector is 0x20 (GDT entry 3). Also sets RPL to 3
	mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  mov 8(%esp), %eax
  mov 4(%esp), %edx       # user entry

	push $0x23		          # SS, notice it uses same selector as above
	push %eax		            # ESP
	pushf			              # push EFLAGS on stack

	pop %eax
	or $0x200, %eax	        # enable IF in EFLAGS
	push %eax

  # mov 4(%esp), %eax
	# set address in user stack which causes the page fault when finishing a user thread
	# sub $4, %eax
	# mov 12(%esp), %ebx
	# mov %ebx, (%eax)

	push $0x1b		          # CS, user mode code selector is 0x18. With RPL 3 this is 0x1b
	push %edx               # EIP first
	iret                    # jump to usermode, iret loads EIP, CS, EFLAGS, ESP, SS from the stack

.global jump_to_process
.type jump_to_process, @function
jump_to_process:
  cli                     # TODO: not sure about this, because https://forum.osdev.org/viewtopic.php?f=1&t=20572
  mov 8(%esp), %ebx       # get proc_stack
  mov 4(%esp), %ecx       # get entry_point
  
	mov $0x23, %ax	        # user mode data selector is 0x20 (GDT entry 3). Also sets RPL to 3
	mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  
  # create stack frame
  push $0x23				# SS, notice it uses same selector as above
  push %ebx		    # stack

  push $0x200

  push $0x1b			  # CS, user mode code selector is 0x18. With RPL 3 this is 0x1b
  push %ecx	        # EIP

  iret
  
.global asm_syscall_print
.type asm_syscall_print, @function
asm_syscall_print:
  mov $512, %eax
  lea [hello_world], %ebx
  int $0x80

.global restore_kernel
.type restore_kernel, @function
restore_kernel:
  cli
  mov $0x10, %ax	     
	mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  sti
  ret