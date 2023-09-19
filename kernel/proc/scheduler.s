.global scheduler_isr
.type scheduler_isr, @function
scheduler_isr:
  cli
  pusha         # if no current task, then just return

  #mov [_currentTask], %eax 
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
  #mov [_currentTask], %eax
  #mov [%eax], %esp

  #call scheduler_tick
  
  # restore esp
  #mov [_currentTask], %eax 
  #mov [%eax], %esp

  # call tss_set_stack (kernelSS, kernelESP).
  # this code will be needed later for user tasks.

  #push dword ptr [eax+8]
  #push dword ptr [eax+12]
  #call tss_set_stack
  add $8, %esp

  # send EOI and restore context.

  pop %gs
  pop %fs
  pop %es
  pop %ds

interrupt_return:
  # test if we need to call old isr
  #mov eax, old_isr
  #cmp eax, 0
  #jne chain_interrupt

  # # if old_isr is null, send EOI and return.
  #mov al, 0x20
  #out 0x20, al

  popa
  iret

chain_interrupt:
  popa
  #jmp old_isr
