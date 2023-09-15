
.section .rodata
.align 0x1000
hello_world:    .asciz "Privet\n"

.section .text
.align 0x1000
_start:
  //print string
  lea hello_world, %ebx
  mov $512, %eax
  int $0x80

  //terminate
  mov $1, %eax
  int $0x80