.section .text
.global _start

_start:
    mov $60, %rax
    mov $42, %rdi
    syscall

