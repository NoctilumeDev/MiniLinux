#ifndef MINILINUX_USER_H
#define MINILINUX_USER_H
#include <stdbool.h>
#include <stddef.h>
#include "task.h"
void user_prepare(void);
bool user_range(const struct task *task, uint64_t address, size_t length, bool writing);
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
struct interrupt_frame *user_fault(struct interrupt_frame *frame, uint64_t address);
__attribute__((noreturn)) void user_complete(void);
#endif
