#ifndef MINILINUX_ABI_H
#define MINILINUX_ABI_H
#include <stdint.h>
enum { SYS_TASK_ID, SYS_WRITE, SYS_TICKS, SYS_REPORT, SYS_EXIT, SYS_OPEN, SYS_READ, SYS_CLOSE };
/* LAB adds a tiny teaching ABI; the M0-M6 numbers stay unchanged. */
enum { SYS_PROGRAM = 8, SYS_SPAWN, SYS_WAIT, SYS_INPUT, SYS_PROCESS, SYS_FILE,
       SYS_MEMORY, SYS_INSPECT };
enum { PROGRAM_INIT, PROGRAM_SHELL, PROGRAM_HELLO, PROGRAM_READER,
       PROGRAM_COUNTER_A, PROGRAM_COUNTER_B, PROGRAM_FAULT, PROGRAM_COUNT };
enum { PROCESS_UNUSED, PROCESS_READY, PROCESS_WAITING, PROCESS_EXITED };
struct process_info {
    uint64_t pid, parent, program, state, root, slices;
};
struct file_info { char name[32]; uint64_t size; };
struct memory_info { uint64_t total, free; };
#define USER_CODE UINT64_C(0x400000)
#define USER_DATA UINT64_C(0x600000)
#define USER_STACK UINT64_C(0x800000)
#define KERNEL_PROBE UINT64_C(0xffffffff80000000)
enum { COPY_LIMIT = 256 };
#endif
