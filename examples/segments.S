.global _start

.section .rodata
msg:    .asciz "Value = "
nl:     .asciz "\n"

.section .data
value:  .quad 7

.section .bss
.lcomm buffer, 32

.section .text

_start:
    /* write(1, msg, 8) */
    mov $1, %rax
    mov $1, %rdi
    lea msg(%rip), %rsi
    mov $8, %rdx
    syscall

    /* load value from .data (relocation test) */
    mov value(%rip), %rax

    /* convert to ascii -> buffer (.bss test) */
    lea buffer(%rip), %rdi
    call itoa

    /* print buffer */
    mov $1, %rax
    mov $1, %rdi
    lea buffer(%rip), %rsi
    mov $32, %rdx
    syscall

    /* print newline (.rodata again) */
    mov $1, %rax
    mov $1, %rdi
    lea nl(%rip), %rsi
    mov $1, %rdx
    syscall

    /* exit */
    mov $60, %rax
    xor %rdi, %rdi
    syscall

itoa:
    mov $10, %rbx
    add $31, %rdi
    movb $0, (%rdi)

.loop:
    xor %rdx, %rdx
    div %rbx
    add $'0', %dl
    dec %rdi
    mov %dl, (%rdi)
    test %rax, %rax
    jne .loop
    ret

