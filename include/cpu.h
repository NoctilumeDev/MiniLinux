#ifndef MINILINUX_CPU_H
#define MINILINUX_CPU_H
#include <stdint.h>

/* This order matches entry.S, followed by the long-mode CPU return frame. */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
};
_Static_assert(sizeof(struct interrupt_frame) == 176, "interrupt frame size");
void cpu_init(void);
void cpu_set_kernel_stack(uint64_t top);
void cpu_start_timer(void);
__attribute__((noreturn)) void cpu_enter_frame(struct interrupt_frame *frame);
struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame);

static inline void port_write(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
#endif
