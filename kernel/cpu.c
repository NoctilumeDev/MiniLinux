#include <stddef.h>
#include "cpu.h"

struct descriptor_pointer { uint16_t limit; uint64_t base; } __attribute__((packed));
struct task_state_segment {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2, reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap;
} __attribute__((packed));
_Static_assert(sizeof(struct task_state_segment) == 104, "TSS size");
struct interrupt_gate {
    uint16_t offset_low, selector;
    uint8_t ist, attributes;
    uint16_t offset_middle;
    uint32_t offset_high, reserved;
} __attribute__((packed));

static struct task_state_segment tss;
static uint64_t gdt[7];
static struct interrupt_gate idt[256];
extern void cpu_load_gdt(const struct descriptor_pointer *pointer);
extern void (*exception_entries[32])(void);
extern void interrupt_32(void), interrupt_128(void), interrupt_255(void);

static void set_gate(unsigned vector, void (*entry)(void), uint8_t attributes) {
    uint64_t address = (uint64_t)(uintptr_t)entry;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = 0x08;
    idt[vector].attributes = attributes;
    idt[vector].offset_middle = (uint16_t)(address >> 16);
    idt[vector].offset_high = (uint32_t)(address >> 32);
}

void cpu_set_kernel_stack(uint64_t top) { tss.rsp0 = top; }

void cpu_init(void) {
    __asm__ volatile ("cli" : : : "memory");
    gdt[1] = UINT64_C(0x00af9a000000ffff); /* 64-bit kernel code. */
    gdt[2] = UINT64_C(0x00cf92000000ffff); /* Kernel data. */
    gdt[3] = UINT64_C(0x00cff2000000ffff); /* User data, selector 0x1b. */
    gdt[4] = UINT64_C(0x00affa000000ffff); /* User code, selector 0x23. */
    uint64_t base = (uint64_t)(uintptr_t)&tss;
    gdt[5] = 103 | ((base & 0xffffff) << 16) | (UINT64_C(0x89) << 40) |
             ((base & UINT64_C(0xff000000)) << 32);
    gdt[6] = base >> 32;
    tss.iomap = sizeof(tss); /* No user access to I/O ports. */
    struct descriptor_pointer gdtr = { sizeof(gdt) - 1, (uint64_t)(uintptr_t)gdt };
    cpu_load_gdt(&gdtr);
    for (unsigned i = 0; i < 256; i++) { set_gate(i, interrupt_255, 0x8e); }
    for (unsigned i = 0; i < 32; i++) { set_gate(i, exception_entries[i], 0x8e); }
    set_gate(32, interrupt_32, 0x8e);
    set_gate(128, interrupt_128, 0xee); /* Ring 3 may request this controlled entry. */
    struct descriptor_pointer idtr = { sizeof(idt) - 1, (uint64_t)(uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}

static void pic_write(uint16_t port, uint8_t value) {
    port_write(port, value);
    port_write(0x80, 0); /* Give the legacy controller an I/O delay. */
}

void cpu_start_timer(void) {
    /* Limine may leave LAPIC enabled with its ExtINT input masked. For this
       single-core PIC lesson, explicitly select the legacy interrupt path. */
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1b));
    low &= ~((1u << 11) | (1u << 10));
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(0x1b));
    pic_write(0x20, 0x11); pic_write(0xa0, 0x11);
    pic_write(0x21, 32); pic_write(0xa1, 40);
    pic_write(0x21, 4); pic_write(0xa1, 2);
    pic_write(0x21, 1); pic_write(0xa1, 1);
    pic_write(0x21, 0xfe); pic_write(0xa1, 0xff); /* Only IRQ0 is unmasked. */
    const uint16_t divisor = 11932; /* PIT input 1,193,182 Hz, about 100 Hz. */
    port_write(0x43, 0x36);
    port_write(0x40, (uint8_t)divisor);
    port_write(0x40, (uint8_t)(divisor >> 8));
}
