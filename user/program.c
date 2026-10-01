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

#if MINILINUX_LEVEL >= 5
static void check(int condition) { if (!condition) { fail(); } }

static uint64_t read_file(void) {
    static const char name[] = "/hello.txt", missing[] = "/missing", empty[] = "/empty.txt";
    static const char expected[] = "hello from ramfs\n";
    char buffer[32];
    uint64_t out = (uint64_t)(uintptr_t)buffer;
    check(call(SYS_OPEN, (uint64_t)(uintptr_t)missing, sizeof(missing) - 1, 0) == -4);
    int64_t first = call(SYS_OPEN, (uint64_t)(uintptr_t)name, sizeof(name) - 1, 0);
    int64_t second = call(SYS_OPEN, (uint64_t)(uintptr_t)name, sizeof(name) - 1, 0);
    check(first == 0 && second == 1);
    check(call(SYS_OPEN, (uint64_t)(uintptr_t)name, sizeof(name) - 1, 0) == -5);
    check(call(SYS_READ, 99, out, 1) == -3);
    check(call(SYS_CLOSE, 99, 0, 0) == -3);
    check(call(SYS_READ, first, USER_CODE, 4) == -2);
    volatile uint8_t *edge = (volatile uint8_t *)(uintptr_t)(USER_DATA + 4096 - 8);
    for (unsigned i = 0; i < 8; i++) { edge[i] = 0xa5; }
    check(call(SYS_READ, first, (uint64_t)(uintptr_t)edge, 16) == -2);
    for (unsigned i = 0; i < 8; i++) { check(edge[i] == 0xa5); }

    check(call(SYS_READ, first, out, 5) == 5);
    for (unsigned i = 0; i < 5; i++) { check(buffer[i] == expected[i]); }
    check(call(SYS_WRITE, out, 5, 0) == 5);
    check(call(SYS_READ, first, out, sizeof(buffer)) == 12);
    for (unsigned i = 0; i < 12; i++) { check(buffer[i] == expected[5 + i]); }
    check(call(SYS_WRITE, out, 12, 0) == 12);
    check(call(SYS_READ, first, out, 1) == 0);
    check(call(SYS_READ, second, out, 1) == 1 && buffer[0] == expected[0]);
    check(call(SYS_CLOSE, first, 0, 0) == 0);
    check(call(SYS_CLOSE, second, 0, 0) == 0);
    check(call(SYS_CLOSE, second, 0, 0) == -3);
    check(call(SYS_READ, second, out, 1) == -3);
    int64_t blank = call(SYS_OPEN, (uint64_t)(uintptr_t)empty, sizeof(empty) - 1, 0);
    check(blank == 0 && call(SYS_READ, blank, out, 1) == 0);
    check(call(SYS_CLOSE, blank, 0, 0) == 0);
    return sizeof(expected) - 1;
}
#endif

__attribute__((section(".text.entry"), noreturn)) void user_main(void) {
    uint64_t id = (uint64_t)call(SYS_TASK_ID, 0, 0, 0);
    static const char greeting[] = "user: syscall returned to CPL3\n";
    if (call(SYS_WRITE, (uint64_t)(uintptr_t)greeting, sizeof(greeting) - 1, 0) !=
        (int64_t)(sizeof(greeting) - 1)) { fail(); }
    if (call(99, 0, 0, 0) != -1) { fail(); }
    if (call(SYS_WRITE, KERNEL_PROBE, 1, 0) != -2) { fail(); }
    if (call(SYS_WRITE, USER_DATA + 4096 - 8, 16, 0) != -2) { fail(); }
    if (call(SYS_WRITE, UINT64_MAX - 3, 8, 0) != -2) { fail(); }

    uint64_t file_length = 0;
#if MINILINUX_LEVEL >= 5
    file_length = read_file();
#endif

    volatile uint64_t *data = (volatile uint64_t *)(uintptr_t)USER_DATA;
    uint64_t start = (uint64_t)call(SYS_TICKS, 0, 0, 0);
    uint64_t duration = id == 0 ? 12 : 24;
    while ((uint64_t)call(SYS_TICKS, 0, 0, 0) - start < duration) {
        for (unsigned i = 0; i < 10000; i++) { data[2 + id]++; }
    }
    if (call(SYS_REPORT, 0x4d0 + MINILINUX_LEVEL, file_length, 0) != 0) { fail(); }
    if (id == 0) {
        /* A real user load, not a kernel check pretending to be a fault. */
        volatile uint8_t denied = *(volatile uint8_t *)(uintptr_t)KERNEL_PROBE;
        (void)denied;
        fail();
    }
    call(SYS_EXIT, 0, 0, 0);
    for (;;) { }
}
