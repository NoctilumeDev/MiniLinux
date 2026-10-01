#include "console.h"
#include "cpu.h"
#include "task.h"

struct task {
    struct interrupt_frame *frame;
    uint8_t stack[8192] __attribute__((aligned(16)));
    volatile uint64_t counter;
    unsigned slices;
};
static struct task tasks[2];
static unsigned current;

static void count_forever(void) {
    unsigned owner = current;
    uint64_t expected = UINT64_C(0xcafe0000) + owner;
    /* Observe the live register, rather than reloading a C local each loop. */
    __asm__ volatile ("mov %0, %%r12" : : "r"(expected) : "r12");
    for (;;) {
        uint64_t guard;
        __asm__ volatile ("mov %%r12, %0" : "=r"(guard));
        if (guard != expected) { panic("M3 register guard changed"); }
        tasks[owner].counter++;
    }
}

__attribute__((noreturn)) void task_start(void) {
    for (unsigned i = 0; i < 2; i++) {
        uint64_t top = (uint64_t)(uintptr_t)&tasks[i].stack[sizeof(tasks[i].stack)];
        /* A C function normally starts with RSP == 8 modulo 16. */
        top -= 8;
        tasks[i].frame = (struct interrupt_frame *)(uintptr_t)(top - sizeof(struct interrupt_frame));
        tasks[i].frame->rip = (uint64_t)(uintptr_t)count_forever;
        tasks[i].frame->cs = 0x08;
        tasks[i].frame->ss = 0x10;
        tasks[i].frame->rflags = 0x202;
        tasks[i].frame->rsp = top;
    }
    cpu_set_kernel_stack((uint64_t)(uintptr_t)&tasks[0].stack[8192]);
    cpu_start_timer();
    cpu_enter_frame(tasks[0].frame);
}

struct interrupt_frame *task_tick(struct interrupt_frame *frame) {
    tasks[current].frame = frame;
    tasks[current].slices++;
    unsigned next = 1 - current;
    serial_write("M3 timer switch "); serial_write_number(current);
    serial_write(" -> "); serial_write_number(next);
    serial_write("; counters "); serial_write_number(tasks[0].counter);
    serial_write(" / "); serial_write_number(tasks[1].counter);
    serial_write("\n");
    if (tasks[0].slices >= 4 && tasks[1].slices >= 4) {
        if (tasks[0].counter == 0 || tasks[1].counter == 0) { panic("M3 task never ran"); }
        serial_write("MiniLinux M3: timer preemption checks passed\n");
        panic("M3 reached its intentional stop");
    }
    current = next;
    return tasks[next].frame;
}
