/* The interactive layer is separate from the frozen two-task experiments. */
#include "abi.h"
#include "bytes.h"
#include "console.h"
#include "page.h"
#include "user.h"

enum { TASK_LIMIT = 8, CODE_PAGE_LIMIT = 16 };
static struct task tasks[TASK_LIMIT];
static unsigned current, next_pid = 1;
static uint64_t ticks;
extern const unsigned char user_image[];
extern const uint64_t user_image_size;

struct task *task_current(void) { return &tasks[current]; }
struct task *task_get(unsigned index) { return index < TASK_LIMIT ? &tasks[index] : 0; }
uint64_t task_ticks(void) { return ticks; }

static void event(const char *kind) {
    trace_text("ML\t"); trace_text(kind); trace_text("\t"); trace_number(ticks);
}
static void field(uint64_t value) { trace_text("\t"); trace_number(value); }
static void hex_field(uint64_t value) { trace_text("\t"); trace_hex(value); }
static void end_event(void) { trace_text("\n"); }

static void process_event(const struct task *task) {
    event("process"); field(task->id); field(task->parent); field(task->program);
    field(task->state); hex_field(task->space ? task->space->root : 0);
    field(task->slices); field(task == task_current() && task->state == PROCESS_READY); end_event();
}

static void memory_event(void) {
    struct page_info info;
    page_get_info(&info);
    event("memory"); field(info.frame_count); field(info.free_count); end_event();
}

static void mappings_event(const struct task *task) {
    if (!task->space || !task->space->root) { return; }
    unsigned code_pages = (unsigned)((user_image_size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (unsigned i = 0; i < code_pages + 3; i++) {
        uint64_t va = i < code_pages ? USER_CODE + i * PAGE_SIZE :
            i == code_pages ? USER_DATA : USER_STACK + (i - code_pages - 1) * PAGE_SIZE;
        uint64_t pa = 0;
        unsigned flags = 0;
        if (!vm_lookup(task->space, va, &pa, &flags)) { panic("LAB mapping disappeared"); }
        event("map"); field(task->id); hex_field(va); hex_field(pa); field(flags);
        trace_text(i < code_pages ? "\tcode" : i == code_pages ? "\tdata" : "\tstack");
        end_event();
    }
    event("data"); field(task->id); hex_field(USER_DATA);
    hex_field(task->data_page); hex_field(*(uint64_t *)page_data(task->data_page)); end_event();
}

static void inspect(void) {
    event("snapshot"); end_event();
    for (unsigned i = 0; i < TASK_LIMIT; i++) {
        if (tasks[i].state != PROCESS_UNUSED) {
            process_event(&tasks[i]); mappings_event(&tasks[i]);
        }
    }
    memory_event();
    event("snapshot-end"); end_event();
}

static bool map_owned(struct task *task, uint64_t va, unsigned flags, uint64_t *pa) {
    if (task->page_count == 19 || !page_alloc(pa)) { return false; }
    task->pages[task->page_count++] = *pa;
    memset(page_data(*pa), 0, PAGE_SIZE);
    return vm_map(task->space, va, *pa, flags);
}

static void release_pages(struct task *task) {
    if (task->space && task->space->root && !vm_destroy(task->space)) {
        panic("LAB tried to release an active root");
    }
    for (unsigned i = 0; i < task->page_count; i++) {
        if (!page_free(task->pages[i])) { panic("LAB user page ownership differs"); }
    }
    task->page_count = 0;
}

static bool prepare(struct task *task) {
    task->space = &task->private_space;
    if (!vm_create(task->space)) { return false; }
    unsigned count = (unsigned)((user_image_size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (unsigned i = 0; i < count; i++) {
        uint64_t pa = 0, offset = i * PAGE_SIZE, length = user_image_size - offset;
        if (length > PAGE_SIZE) { length = PAGE_SIZE; }
        if (!map_owned(task, USER_CODE + offset, VM_USER | VM_EXEC, &pa)) { return false; }
        memcpy(page_data(pa), user_image + offset, length);
    }
    uint64_t stack = 0;
    if (!map_owned(task, USER_DATA, VM_USER | VM_WRITE, &task->data_page) ||
        !map_owned(task, USER_STACK, VM_USER | VM_WRITE, &stack) ||
        !map_owned(task, USER_STACK + PAGE_SIZE, VM_USER | VM_WRITE, &stack)) { return false; }
    uint64_t top = (uint64_t)(uintptr_t)&task->stack[sizeof(task->stack)];
    task->frame = (struct interrupt_frame *)(uintptr_t)(top - sizeof(struct interrupt_frame));
    memset(task->frame, 0, sizeof(*task->frame));
    task->frame->rip = USER_CODE;
    task->frame->cs = 0x23; task->frame->ss = 0x1b;
    task->frame->rsp = USER_STACK + 2 * PAGE_SIZE - 8;
    task->frame->rflags = 0x202;
    return true;
}

static int64_t spawn(unsigned program, unsigned parent) {
    for (unsigned i = 0; i < TASK_LIMIT; i++) {
        struct task *task = &tasks[i];
        if (task->state != PROCESS_UNUSED) { continue; }
        memset(task, 0, sizeof(*task));
        task->id = next_pid++; task->program = program; task->parent = parent;
        if (!prepare(task)) { release_pages(task); memset(task, 0, sizeof(*task)); return -5; }
        task->state = PROCESS_READY; task->alive = true;
        event("spawn"); field(task->id); field(parent); field(program); end_event();
        process_event(task); mappings_event(task); memory_event();
        return task->id;
    }
    return -5;
}

static unsigned next_ready(void) {
    for (unsigned n = 1; n <= TASK_LIMIT; n++) {
        unsigned i = (current + n) % TASK_LIMIT;
        if (tasks[i].state == PROCESS_READY) { return i; }
    }
    panic("LAB has no runnable shell");
}

static struct interrupt_frame *select_task(unsigned next) {
    current = next;
    struct task *task = task_current();
    cpu_set_kernel_stack((uint64_t)(uintptr_t)&task->stack[sizeof(task->stack)]);
    if (vm_current_root() != task->space->root) { vm_switch(task->space->root); }
    return task->frame;
}

__attribute__((noreturn)) void task_start(void) {
    if (!user_image_size || user_image_size > CODE_PAGE_LIMIT * PAGE_SIZE) {
        panic("LAB embedded image is too large");
    }
    event("boot"); trace_text("\tLAB\tx86_64\t100\t8"); end_event();
    if (spawn(PROGRAM_INIT, 0) != 1) { panic("LAB init creation failed"); }
    struct interrupt_frame *frame = select_task(0);
    cpu_start_timer();
    cpu_enter_frame(frame);
}

struct interrupt_frame *task_tick(struct interrupt_frame *frame) {
    if ((frame->cs & 3) != 3) { panic("LAB timer must preempt user mode"); }
    tasks[current].frame = frame; tasks[current].slices++; ticks++;
    unsigned next = next_ready();
    /* Record actual changes, not every tick of an idle shell. */
    if (next != current) {
        event("schedule"); field(tasks[current].id); field(tasks[next].id);
        hex_field(tasks[next].space->root); field(frame->cs & 3); end_event();
    }
    return select_task(next);
}

struct interrupt_frame *task_exit(struct interrupt_frame *frame, uint64_t status) {
    struct task *old = task_current();
    if (old->program <= PROGRAM_SHELL) { panic("LAB init or shell exited"); }
    old->frame = frame; old->status = status; old->state = PROCESS_EXITED; old->alive = false;
    event("exit"); field(old->id); field(status); end_event();
    bool reaped = false;
    for (unsigned i = 0; i < TASK_LIMIT; i++) {
        struct task *parent = &tasks[i];
        if (parent->id == old->parent && parent->state == PROCESS_WAITING &&
            parent->waiting_for == old->id) {
            parent->frame->rax = status; parent->state = PROCESS_READY; reaped = true;
            process_event(parent);
        }
    }
    /* Kernel stacks remain fixed; switch roots before releasing user mappings. */
    struct interrupt_frame *next = select_task(next_ready());
    release_pages(old);
    if (reaped) { old->state = PROCESS_UNUSED; }
    process_event(old); memory_event();
    return next;
}

struct interrupt_frame *user_fault(struct interrupt_frame *frame, uint64_t address) {
    event("fault"); field(task_current()->id); field(frame->vector);
    field(frame->error); hex_field(address); hex_field(frame->rip); field(frame->cs & 3); end_event();
    serial_write("\nuser fault: pid="); serial_write_number(task_current()->id);
    serial_write(" vector="); serial_write_number(frame->vector); serial_write("\n");
    return task_exit(frame, 128 + frame->vector);
}

static struct interrupt_frame *wait_child(struct interrupt_frame *frame, uint64_t pid) {
    struct task *parent = task_current();
    event("wait"); field(parent->id); field(pid); end_event();
    for (unsigned i = 0; i < TASK_LIMIT; i++) {
        struct task *child = &tasks[i];
        if (child->state == PROCESS_UNUSED || child->id != pid || child->parent != parent->id) { continue; }
        if (child->state == PROCESS_EXITED) {
            frame->rax = child->status; child->state = PROCESS_UNUSED; process_event(child); return frame;
        }
        parent->frame = frame; parent->state = PROCESS_WAITING; parent->waiting_for = (unsigned)pid;
        process_event(parent);
        return select_task(next_ready());
    }
    frame->rax = (uint64_t)-3;
    return frame;
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame) {
    if (frame->cs != 0x23) { panic("LAB syscall must enter from CPL3"); }
    struct task *task = task_current();
    uint64_t number = frame->rax;
    int64_t result = -1;
    switch (number) {
    case SYS_TASK_ID: result = task->id; break;
    case SYS_PROGRAM: result = task->program; break;
    case SYS_TICKS: result = (int64_t)ticks; break;
    case SYS_WRITE:
        if (!user_range(task, frame->rdi, frame->rsi, false)) { result = -2; break; }
        serial_write_bytes((const char *)(uintptr_t)frame->rdi, frame->rsi);
        result = (int64_t)frame->rsi; break;
    case SYS_OPEN:
        result = user_range(task, frame->rdi, frame->rsi, false) ?
            ramfs_open(task->files, (const char *)(uintptr_t)frame->rdi, frame->rsi) : -2; break;
    case SYS_READ:
        result = user_range(task, frame->rsi, frame->rdx, true) ?
            ramfs_read(task->files, frame->rdi, (char *)(uintptr_t)frame->rsi, frame->rdx) : -2; break;
    case SYS_CLOSE: result = ramfs_close(task->files, frame->rdi); break;
    case SYS_SPAWN:
        if ((task->program == PROGRAM_INIT && frame->rdi == PROGRAM_SHELL) ||
            (task->program == PROGRAM_SHELL && frame->rdi >= PROGRAM_HELLO && frame->rdi < PROGRAM_COUNT)) {
            result = spawn((unsigned)frame->rdi, task->id);
        } else { result = -3; }
        break;
    case SYS_WAIT: return wait_child(frame, frame->rdi);
    case SYS_EXIT: return task_exit(frame, frame->rdi);
    case SYS_INPUT:
        if (!user_range(task, frame->rdi, 1, true)) { result = -2; break; }
        result = serial_read_char();
        if (result < 0) { result = -6; break; }
        *(uint8_t *)(uintptr_t)frame->rdi = (uint8_t)result;
        result = 1; break;
    case SYS_PROCESS:
        if (!user_range(task, frame->rsi, sizeof(struct process_info), true)) { result = -2; break; }
        if (frame->rdi >= TASK_LIMIT || tasks[frame->rdi].state == PROCESS_UNUSED) { result = 0; break; }
        {
            const struct task *found = &tasks[frame->rdi];
            struct process_info info = { found->id, found->parent, found->program, found->state,
                                        found->space ? found->space->root : 0, found->slices };
            memcpy((void *)(uintptr_t)frame->rsi, &info, sizeof(info)); result = 1;
        }
        break;
    case SYS_FILE:
        if (!user_range(task, frame->rsi, sizeof(struct file_info), true)) { result = -2; break; }
        result = frame->rdi > UINT32_MAX ? 0 : ramfs_file((unsigned)frame->rdi, (void *)(uintptr_t)frame->rsi); break;
    case SYS_MEMORY:
        if (!user_range(task, frame->rdi, sizeof(struct memory_info), true)) { result = -2; break; }
        {
            struct page_info info; page_get_info(&info);
            struct memory_info out = { info.frame_count, info.free_count };
            memcpy((void *)(uintptr_t)frame->rdi, &out, sizeof(out)); result = 0;
        }
        break;
    case SYS_INSPECT: inspect(); result = 0; break;
    }
    /* Polling and individual echo characters are intentionally not a trace flood. */
    if (number != SYS_TICKS && number != SYS_INSPECT && number != SYS_TASK_ID &&
        number != SYS_PROGRAM && !(number == SYS_INPUT && result == -6) &&
        !(number == SYS_WRITE && frame->rsi == 1)) {
        event("syscall"); field(task->id); field(number);
        hex_field(frame->rdi); hex_field(frame->rsi); hex_field(frame->rdx);
        trace_text("\t");
        if (result < 0) { trace_text("-"); trace_number((uint64_t)-result); }
        else { trace_number((uint64_t)result); }
        field(3); end_event();
    }
    frame->rax = (uint64_t)result;
    return frame;
}
