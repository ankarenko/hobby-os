
.section .rodata
.align 0x1000
hello_world:    .asciz "Privet\n"

.section .text
.align 0x1000
_start:
  lea [hello_world], %ebx
  mov $512, %eax
  int $0x80