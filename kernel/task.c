#include "console.h"
#include "cpu.h"
#include "task.h"
#if MINILINUX_LEVEL >= 4
#include "user.h"
#endif
static struct task tasks[2];
static unsigned current;
static uint64_t ticks;

struct task *task_current(void) { return &tasks[current]; }
struct task *task_get(unsigned index) { return index < 2 ? &tasks[index] : 0; }
uint64_t task_ticks(void) { return ticks; }

#if MINILINUX_LEVEL == 3
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
#endif

static struct interrupt_frame *select_task(unsigned next) {
    current = next;
    cpu_set_kernel_stack((uint64_t)(uintptr_t)&tasks[next].stack[8192]);
    if (tasks[next].space != 0 && vm_current_root() != tasks[next].space->root) {
        vm_switch(tasks[next].space->root);
    }
    return tasks[next].frame;
}

__attribute__((noreturn)) void task_start(void) {
    for (unsigned i = 0; i < 2; i++) { tasks[i].id = i; tasks[i].alive = true; }
#if MINILINUX_LEVEL >= 4
    user_prepare();
#else
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
#endif
    struct interrupt_frame *first = select_task(0);
    cpu_start_timer();
    cpu_enter_frame(first);
}

struct interrupt_frame *task_tick(struct interrupt_frame *frame) {
    tasks[current].frame = frame;
    tasks[current].slices++;
    ticks++;
    unsigned next = tasks[1 - current].alive ? 1 - current : current;
#if MINILINUX_LEVEL == 3
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
#else
    if ((frame->cs & 3) != 3) { panic("timer did not interrupt user code"); }
    if (ticks <= 6 || ticks % 8 == 0) {
        serial_write("user timer task "); serial_write_number(current);
        serial_write(" -> "); serial_write_number(next);
        serial_write(" CS="); serial_write_hex(frame->cs);
        serial_write(" CR3="); serial_write_hex(vm_current_root());
        serial_write("\n");
    }
#endif
    return select_task(next);
}

#if MINILINUX_LEVEL >= 4
struct interrupt_frame *task_exit(struct interrupt_frame *frame, uint64_t status) {
    if (status == 2) { panic("user address space marker changed"); }
    if (status != 0) { panic("user program reported failure"); }
    tasks[current].frame = frame;
    tasks[current].alive = false;
    serial_write("task stopped: "); serial_write_number(current); serial_write("\n");
    unsigned next = 1 - current;
    if (!tasks[next].alive) { user_complete(); }
    return select_task(next);
}
#endif
