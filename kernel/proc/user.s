.global enter_usermode
.type enter_usermode, @function
enter_usermode:
	cli                     # TODO: not sure about this, because https://forum.osdev.org/viewtopic.php?f=1&t=20572

	mov $0x23, %ax	        # user mode data selector is 0x20 (GDT entry 3). Also sets RPL to 3
	mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

	push $0x23		          # SS, notice it uses same selector as above
	push %esp		            # ESP
	pushf			              # push EFLAGS on stack

	pop %eax
	or $0x200, %eax	        # enable IF in EFLAGS
	push %eax

	push $0x1b		          # CS, user mode code selector is 0x18. With RPL 3 this is 0x1b
	lea [user_start], %eax  # EIP first
	push %eax
	iret                    # jump to usermode, iret loads EIP, CS, EFLAGS, ESP, SS from the stack

user_start:
  add $4, %esp            #point esp to return address (?)
  ret                     #restore EIP from the stack
                          #(return back to C code where we invoked enter_usermode)