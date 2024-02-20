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

  ## if old_scheduler_isr is null, send EOI and return.
  mov $0x20, %al 
  out %al, $0x20

  #mov 4(%esp), %eax
	# set address in user stack which causes the page fault when finishing a user struct thread
	#sub $4, %eax
	#mov 4(%ebp), %ebx
	#mov %ebx, (%eax)

	push $0x1b		          # CS, user mode code selector is 0x18. With RPL 3 this is 0x1b
	push %edx               # EIP first
	iret                    # jump to usermode, iret loads EIP, CS, EFLAGS, ESP, SS from the stack


.global return_usermode
.type return_usermode, @function
return_usermode:
	cli

	mov $0x23, %ax	          # user mode data selector is 0x20 (GDT entry 3). Also sets RPL to 3
	mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs              # we don't need to worry about SS. it's handled by iret

	mov 4(%esp), %eax

  push 72(%eax) # user data segment
  push 68(%eax) # push our current stack
  push 64(%eax) # EFLAGS
  push 60(%eax) # segment selector
  push 56(%eax) # eip

  mov 16(%eax), %edi
  mov 20(%eax), %esi
  mov 24(%eax), %ebp
  #mov 28(%eax), %esp
  mov 32(%eax), %ebx
  mov 36(%eax), %edx
  mov 40(%eax), %ecx
  mov 44(%eax), %eax

	iret
