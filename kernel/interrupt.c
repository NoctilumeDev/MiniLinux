#include "console.h"
#include "cpu.h"
#include "task.h"
#if MINILINUX_LEVEL >= 4
#include "user.h"
#endif

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame) {
#if MINILINUX_LEVEL >= 4
    if (frame->vector == 128) { return syscall_dispatch(frame); }
    if (frame->vector == 14 && (frame->cs & 3) == 3) {
        uint64_t address;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
        return user_fault(frame, address);
    }
#endif
    if (frame->vector == 32) {
        port_write(0x20, 0x20); /* Acknowledge IRQ before returning to any task. */
        return task_tick(frame);
    }
    serial_write("CPU exception vector="); serial_write_number(frame->vector);
    serial_write(" error="); serial_write_hex(frame->error);
    serial_write(" RIP="); serial_write_hex(frame->rip);
    serial_write(" CS="); serial_write_hex(frame->cs);
    if (frame->vector == 14) {
        uint64_t address;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
        serial_write(" CR2="); serial_write_hex(address);
    }
    serial_write("\n");
    panic("unexpected CPU exception");
}
