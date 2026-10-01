#include <stddef.h>
#include "abi.h"

/* Our tiny ABI, not the Linux syscall ABI. All entry details stay here. */
static int64_t call(uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(a), "S"(b), "d"(c) : "memory", "cc");
    return (int64_t)number;
}

__attribute__((noreturn)) static void fail(void) {
    call(SYS_EXIT, 1, 0, 0);
    for (;;) { }
}

__attribute__((section(".text.entry"), noreturn)) void user_main(void) {
    uint64_t id = (uint64_t)call(SYS_TASK_ID, 0, 0, 0);
    static const char greeting[] = "user: syscall returned to CPL3\n";
    if (call(SYS_WRITE, (uint64_t)(uintptr_t)greeting, sizeof(greeting) - 1, 0) !=
        (int64_t)(sizeof(greeting) - 1)) { fail(); }
    if (call(99, 0, 0, 0) != -1) { fail(); }
    if (call(SYS_WRITE, KERNEL_PROBE, 1, 0) != -2) { fail(); }
    if (call(SYS_WRITE, USER_DATA + 4096 - 8, 16, 0) != -2) { fail(); }
    if (call(SYS_WRITE, UINT64_MAX - 3, 8, 0) != -2) { fail(); }

    volatile uint64_t *data = (volatile uint64_t *)(uintptr_t)USER_DATA;
    uint64_t start = (uint64_t)call(SYS_TICKS, 0, 0, 0);
    uint64_t duration = id == 0 ? 12 : 24;
    while ((uint64_t)call(SYS_TICKS, 0, 0, 0) - start < duration) {
        for (unsigned i = 0; i < 10000; i++) { data[2 + id]++; }
    }
    if (call(SYS_REPORT, 0x4d4, 0, 0) != 0) { fail(); }
    if (id == 0) {
        /* A real user load, not a kernel check pretending to be a fault. */
        volatile uint8_t denied = *(volatile uint8_t *)(uintptr_t)KERNEL_PROBE;
        (void)denied;
        fail();
    }
    call(SYS_EXIT, 0, 0, 0);
    for (;;) { }
}
