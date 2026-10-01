#ifndef MINILINUX_TASK_H
#define MINILINUX_TASK_H
#include "cpu.h"
__attribute__((noreturn)) void task_start(void);
struct interrupt_frame *task_tick(struct interrupt_frame *frame);
#endif
