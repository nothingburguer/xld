.section .text
.global _start

_start:
    mov $1, %rax
    mov $1, %rdi
    lea msg1(%rip), %rsi
    mov $6, %rdx
    syscall

    mov $1, %rax
    mov $1, %rdi
    lea msg2(%rip), %rsi
    mov $6, %rdx
    syscall

    mov $60, %rax
    xor %rdi, %rdi
    syscall

msg1:
    .ascii "Hello\n"
msg2:
    .ascii "World\n"

