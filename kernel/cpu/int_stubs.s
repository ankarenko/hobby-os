.macro ISR_NOERRCODE i
  .global isr\i
  .type isr\i, @function
  isr\i:
    push $0
    push $\i
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE i
  .global isr\i
  .type isr\i, @function
  isr\i:
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

  cld # C code following the sysV ABI requires DF to be clear on function entry
    
  push %esp
  call isr_handler
  #call signal_handler
  
  add $4, %esp    # cleans esp param from the stack

  pop %gs
  pop %fs
  pop %es
  pop %ds

  popa

  add $8, %esp     # Cleans up the pushed error code and pushed ISR number
  iret             # pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP


.macro IRQ i r
  .global irq\i
  .type irq\i, @function
  irq\i:
    push $0
    push $\r
    jmp irq_common_stub
.endm

.global irq_common_stub
.type irq_common_stub, @function
irq_common_stub:
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

  cld # C code following the sysV ABI requires DF to be clear on function entry
    
  push %esp
  
  call irq_handler
  #call signal_handler
  
  add $4, %esp    # cleans esp param from the stack

  pop %gs
  pop %fs
  pop %es
  pop %ds

  popa

  add $8, %esp     # Cleans up the pushed error code and pushed ISR number
  iret

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

// SYS CALL INTERRUPT
ISR_NOERRCODE 128

IRQ   0,    32
IRQ   1,    33
IRQ   2,    34
IRQ   3,    35
IRQ   4,    36
IRQ   5,    37
IRQ   6,    38
IRQ   7,    39
IRQ   8,    40
IRQ   9,    41
IRQ  10,    42
IRQ  11,    43
IRQ  12,    44
IRQ  13,    45
IRQ  14,    46
IRQ  15,    47