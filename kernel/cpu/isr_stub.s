.macro ISR_NOERRCODE i
  .global isr\i
  .type isr\i, @function
  isr\i:
    cli
    push $0
    push $\i
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE i
  .global isr\i
  .type isr\i, @function
  isr\i:
    cli
    push $\i
    jmp isr_common_stub
.endm


# This is our common ISR stub. It saves the processor state, sets
# up for kernel mode segments, calls the C-level fault handler,
# and finally restores the stack frame.
.global isr_common_stub
.type isr_common_stub, @function
isr_common_stub:
  pusha

  push %ds
  push %es
  push %fs
  push %gs

  mov $0x10, %ax    # load the kernel data segment descriptor
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs

  push %esp
  call isr_handler
  
  add $4, %esp    # cleans esp param from the stack

  pop %gs
  pop %fs
  pop %es
  pop %ds

  popa

  add $8, %esp     # Cleans up the pushed error code and pushed ISR number
  iret             # pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

# Only interrupts 8, 10-14 push error codes onto the stack 
# http://www.jamesmolloy.co.uk/tutorial_html/4.-The%20GDT%20and%20IDT.html
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31