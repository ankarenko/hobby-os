.global scheduler_isr
.type scheduler_isr, @function
scheduler_isr:
  # push error codes to comply with interrupt_registers* structe
  push $0 // no error
  push $128 // int_num

  pusha         

  # we are accesing 0 byte at address of current task, which is esp
  #mov [_current_thread], %eax
  #mov (%eax), %eax 
  #cmp $0, %eax
  #jz  interrupt_return

  push %ds
  push %es
  push %fs
  push %gs

  mov $0x10, %ax    # load the kernel data segment descriptor
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  call scheduler_tick
.global scheduler_end
.type scheduler_end, @function
scheduler_end:

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

  ## if old_scheduler_isr is null, send EOI and return.
  mov $0x20, %al 
  out %al, $0x20

  popa
  add $0x8, %esp

  iret

chain_interrupt:
  
  popa
  # remove error codes
  add $0x8, %esp

  # simple jmp [old_pic_isr] did't work for me
  push [old_pic_isr]
  ret

# C declaration:
#   void switch_to_thread(thread_control_block *next_thread)#
#
# WARNING: Caller is expected to disable IRQs before calling, and enable IRQs again after function returns
.global switch_to_thread
.type switch_to_thread, @function
switch_to_thread:
  #Save previous task's state

  #Notes:
  #  For cdecl# EAX, ECX, and EDX are already saved by the caller and don't need to be saved again
  #  EIP is already saved on the stack by the caller's "CALL" instruction
  #  The task isn't able to change CR3 so it doesn't need to be saved
  #  Segment registers are constants (while running kernel code) so they don't need to be saved

  push %ebx
  push %esi
  push %edi
  push %ebp
  
  mov [_current_thread], %edi        # edi = address of the previous task's "thread control block"
  
  mov $4, %eax
  mov %esp, (%edi, %eax, 4)        # Save ESP for previous task's kernel stack in the thread's TCB

  #Load next task's state
  mov $(4 + 1), %eax
  mov (%esp, %eax, 4), %esi        # esi = address of the next task's "thread control block" (parameter passed on stack)
  mov %esi, [_current_thread]        # Current task's TCB is the next task TCB

  mov $4, %eax
  mov (%esi, %eax, 4), %esp        # Load ESP for next task's kernel stack from the thread's TCB
  mov (%esi), %ebx                 # ebx = address for the top of the next task's kernel stack

  push %ebx 
  push $0x10 
  call tss_set_stack
  add $8, %esp                     # remove params from the stack
  mov %cr3, %ecx                   # ecx = previous task's virtual address space
  
  mov $4, %eax
  mov 4(%esi, %eax, 4), %esi       
  mov 4(%esi, %eax, 2), %eax        # eax = address of page directory (phys) for next task

  #push $0
  #push %eax
  #call vmm_get_physical_address
  #add $8, %esp                     # remove params from the stack

  cmp %eax, %ecx                   # Does the virtual address space need to being changed?
  je .doneVAS                      # no, virtual address space is the same, so don't reload it and cause TLB flushes
  mov %eax, %cr3                   # yes, load the next task's virtual address space

.doneVAS:
  pop %ebp
  pop %edi
  pop %esi
  pop %ebx

  ret                              # Load next task's EIP from its kernel stack


.global start_kernel_task
.type start_kernel_task, @function
start_kernel_task:
  mov 4(%esp), %ecx        # esi = address of the next task's "thread control block" (parameter passed on stack)
  mov $4, %eax
  mov (%ecx, %eax, 4), %esp        # Load ESP for next task's kernel stack from the thread's TCB
  jmp .doneVAS


.global get_return_address
.type get_return_address, @function
get_return_address:
  mov 4(%ebp), %eax
  ret
