#include "abi.h"
#include "console.h"
#include "user.h"

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame) {
    struct task *task = task_current();
    if (frame->cs != 0x23) { panic("syscall did not come from CPL3"); }
    if (!task->user_seen) {
        serial_write("syscall from task "); serial_write_number(task->id);
        serial_write(" CS="); serial_write_hex(frame->cs);
        serial_write(" user RSP="); serial_write_hex(frame->rsp); serial_write("\n");
        task->user_seen = true;
    }
    int64_t result = 0;
    switch (frame->rax) {
    case SYS_TASK_ID: result = task->id; break;
    case SYS_TICKS: result = (int64_t)task_ticks(); break;
    case SYS_WRITE:
        if (!user_range(task, frame->rdi, frame->rsi, false)) {
            task->rejected_pointers++;
            result = -2;
            break;
        }
        serial_write_bytes((const char *)(uintptr_t)frame->rdi, frame->rsi);
        result = (int64_t)frame->rsi;
        break;
    case SYS_REPORT:
        if (frame->rdi != 0x4d0 + MINILINUX_LEVEL || task->reported) { panic("user report differs"); }
#if MINILINUX_LEVEL >= 5
        if (frame->rsi != 17) { panic("user did not report the complete file"); }
#endif
        if (task->id == 1 && !task_get(0)->fault_seen) { panic("survivor has not run after fault"); }
        task->reported = true;
        serial_write("user report accepted: "); serial_write_number(task->id); serial_write("\n");
        break;
    case SYS_EXIT: return task_exit(frame, frame->rdi);
#if MINILINUX_LEVEL >= 5
    case SYS_OPEN:
        if (!user_range(task, frame->rdi, frame->rsi, false)) {
            task->rejected_pointers++; result = -2; break;
        }
        result = ramfs_open(task->files, (const char *)(uintptr_t)frame->rdi, frame->rsi);
        if (result >= 0) { task->files_opened++; } else { task->file_errors++; }
        break;
    case SYS_READ:
        /* Validate the entire output BEFORE writing bytes or consuming offset. */
        if (!user_range(task, frame->rsi, frame->rdx, true)) {
            task->rejected_pointers++; result = -2; break;
        }
        result = ramfs_read(task->files, frame->rdi, (char *)(uintptr_t)frame->rsi, frame->rdx);
        if (result > 0) { task->bytes_read += (unsigned)result; }
        else if (result == 0) { task->eof_reads++; }
        else { task->file_errors++; }
        break;
    case SYS_CLOSE:
        result = ramfs_close(task->files, frame->rdi);
        if (result < 0) { task->file_errors++; }
        break;
#endif
    default: task->rejected_calls++; result = -1; break;
    }
    frame->rax = (uint64_t)result;
    return frame;
}
