.macro IRQ i r
  .global irq\i
  .type irq\i, @function
  irq\i:
    cli
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

  push %esp
  call irq_handler
  
  add $4, %esp    # cleans esp param from the stack

  pop %gs
  pop %fs
  pop %es
  pop %ds

  popa

  add $8, %esp     # Cleans up the pushed error code and pushed ISR number
  iret

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