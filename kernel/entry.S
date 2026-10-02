/* CPU entry is assembly; the dispatch and scheduling policy are C. */
.text
.global cpu_load_gdt
cpu_load_gdt:
    lgdt (%rdi)
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    mov $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x28, %eax
    ltr %ax
    ret

/* Only repeat the identical stub; error-code exceptions already push a code. */
.macro INTERRUPT number, error=0
.global interrupt_\number
interrupt_\number:
.if \error == 0
    pushq $0
.endif
    pushq $\number
    jmp interrupt_common
.endm
INTERRUPT 0, 0
INTERRUPT 1, 0
INTERRUPT 2, 0
INTERRUPT 3, 0
INTERRUPT 4, 0
INTERRUPT 5, 0
INTERRUPT 6, 0
INTERRUPT 7, 0
INTERRUPT 8, 1
INTERRUPT 9, 0
INTERRUPT 10, 1
INTERRUPT 11, 1
INTERRUPT 12, 1
INTERRUPT 13, 1
INTERRUPT 14, 1
INTERRUPT 15, 0
INTERRUPT 16, 0
INTERRUPT 17, 1
INTERRUPT 18, 0
INTERRUPT 19, 0
INTERRUPT 20, 0
INTERRUPT 21, 1
INTERRUPT 22, 0
INTERRUPT 23, 0
INTERRUPT 24, 0
INTERRUPT 25, 0
INTERRUPT 26, 0
INTERRUPT 27, 0
INTERRUPT 28, 0
INTERRUPT 29, 1
INTERRUPT 30, 1
INTERRUPT 31, 0
INTERRUPT 32
INTERRUPT 128
INTERRUPT 255

interrupt_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    cld
    mov %rsp, %rdi
    andq $-16, %rsp
    call interrupt_dispatch
    mov %rax, %rsp

/* C returns the original frame or another task's frame. */
restore_frame:
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq

.global cpu_enter_frame
cpu_enter_frame:
    mov %rdi, %rsp
    jmp restore_frame

.section .rodata
.global exception_entries
exception_entries:
    .quad interrupt_0
    .quad interrupt_1
    .quad interrupt_2
    .quad interrupt_3
    .quad interrupt_4
    .quad interrupt_5
    .quad interrupt_6
    .quad interrupt_7
    .quad interrupt_8
    .quad interrupt_9
    .quad interrupt_10
    .quad interrupt_11
    .quad interrupt_12
    .quad interrupt_13
    .quad interrupt_14
    .quad interrupt_15
    .quad interrupt_16
    .quad interrupt_17
    .quad interrupt_18
    .quad interrupt_19
    .quad interrupt_20
    .quad interrupt_21
    .quad interrupt_22
    .quad interrupt_23
    .quad interrupt_24
    .quad interrupt_25
    .quad interrupt_26
    .quad interrupt_27
    .quad interrupt_28
    .quad interrupt_29
    .quad interrupt_30
    .quad interrupt_31
.section .note.GNU-stack,"",@progbits
