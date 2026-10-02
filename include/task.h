#ifndef MINILINUX_TASK_H
#define MINILINUX_TASK_H
#include "cpu.h"
#include "vm.h"
#include "ramfs.h"
struct task {
    struct interrupt_frame *frame;
    uint8_t stack[8192] __attribute__((aligned(16)));
    volatile uint64_t counter;
    unsigned slices;
    struct address_space *space;
    uint64_t data_page;
    unsigned id, rejected_pointers, rejected_calls;
    bool alive, reported, fault_seen, user_seen;
    struct open_file files[OPEN_LIMIT];
    unsigned files_opened, bytes_read, eof_reads, file_errors;
#if MINILINUX_LEVEL == 7
    unsigned parent, program, state, waiting_for;
    uint64_t status;
    struct address_space private_space;
    uint64_t pages[19]; /* 16 code pages, one data page, two stack pages. */
    unsigned page_count;
#endif
};
struct task *task_current(void);
struct task *task_get(unsigned index);
uint64_t task_ticks(void);
__attribute__((noreturn)) void task_start(void);
struct interrupt_frame *task_tick(struct interrupt_frame *frame);
struct interrupt_frame *task_exit(struct interrupt_frame *frame, uint64_t status);
#endif
