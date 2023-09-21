.global scheduler_isr
.type scheduler_isr, @function
scheduler_isr:
  cli
  pusha         # if no current task, then just return

  // we are accesing 0 byte at address of current task, which is esp
  mov [_current_task], %eax
  mov (%eax), %eax 
  cmp $0, %eax
  jz  interrupt_return

  push %ds
  push %es
  push %fs
  push %gs

  mov $0x10, %ax    # load the kernel data segment descriptor
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  cld # C code following the sysV ABI requires DF to be clear on function entry
    
  # save stack and call scheduler 
  mov [_current_task], %eax
  mov %esp, (%eax) 

  call scheduler_tick
  
  # restore esp
  mov [_current_task], %eax 
  mov (%eax), %esp

  # call tss_set_stack (kernelSS, kernelESP).
  # this code will be needed later for user tasks.

  push 8(%eax)  # kernel esp
  push 12(%eax) # kernel ss
  call tss_set_stack
  add $8, %esp // remove params from the stack

  # send EOI and restore context.

  pop %gs
  pop %fs
  pop %es
  pop %ds

interrupt_return:
  # test if we need to call old isr
  mov [old_pic_isr], %eax 
  cmp $0, %eax
  jne chain_interrupt

  # # if old_scheduler_isr is null, send EOI and return.
  mov $0x20, %al 
  out %al, $0x20

  popa
  iret

chain_interrupt:
  popa
  // alternative to jmp [old_pic_isr] it did not work for me though
  push [old_pic_isr]
  ret


.global idle_task
.type idle_task, @function
idle_task:
  push %ebp
  mov %esp, %ebp
again:  
  pause
  jmp again

  leave
  ret

