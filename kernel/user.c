#include "abi.h"
#include "bytes.h"
#include "console.h"
#include "page.h"
#include "user.h"
#if MINILINUX_ATTACK != 0
#include "counterexample.h"
#endif

extern const unsigned char user_image[];
extern const uint64_t user_image_size;
static struct address_space spaces[2];
static uint64_t owned_pages[10]; /* At most five ordinary pages per task. */
static unsigned owned_count;
static uint64_t boot_root, initial_free;

static uint64_t map_page(struct address_space *space, uint64_t address, unsigned flags) {
    uint64_t physical = 0;
    if (owned_count == 10 || !page_alloc(&physical)) { panic("user data allocation failed"); }
    owned_pages[owned_count++] = physical;
    memset(page_data(physical), 0, PAGE_SIZE);
    if (!vm_map(space, address, physical, flags)) { panic("user mapping failed"); }
    return physical;
}

static void load_code(struct address_space *space) {
    for (unsigned i = 0; i < 2; i++) {
        uint64_t code = map_page(space, USER_CODE + i * PAGE_SIZE, VM_USER | VM_EXEC);
        uint64_t offset = i * PAGE_SIZE;
        if (offset < user_image_size) {
            uint64_t length = user_image_size - offset;
            if (length > PAGE_SIZE) { length = PAGE_SIZE; }
            memcpy(page_data(code), &user_image[offset], length);
        }
    }
}

void user_prepare(void) {
    struct page_info info;
    page_get_info(&info);
    initial_free = info.free_count;
    boot_root = vm_current_root();
    if (user_image_size == 0 || user_image_size > 2 * PAGE_SIZE) {
        panic("user image or address space is invalid");
    }
#if MINILINUX_LEVEL < 6
    if (!vm_create(&spaces[0])) { panic("shared user root allocation failed"); }
    load_code(&spaces[0]);
    uint64_t shared_data = map_page(&spaces[0], USER_DATA, VM_USER | VM_WRITE);
#endif
    for (unsigned i = 0; i < 2; i++) {
        struct task *task = task_get(i);
#if MINILINUX_LEVEL >= 6
        task->space = &spaces[i];
        if (!vm_create(task->space)) { panic("private user root allocation failed"); }
        load_code(task->space);
        task->data_page = map_page(task->space, USER_DATA, VM_USER | VM_WRITE);
        uint64_t stack_base = USER_STACK;
#else
        task->space = &spaces[0];
        task->data_page = shared_data;
        uint64_t stack_base = USER_STACK + i * UINT64_C(0x20000);
#endif
        map_page(task->space, stack_base, VM_USER | VM_WRITE);
        map_page(task->space, stack_base + PAGE_SIZE, VM_USER | VM_WRITE);
        uint64_t top = (uint64_t)(uintptr_t)&task->stack[8192];
        task->frame = (struct interrupt_frame *)(uintptr_t)(top - sizeof(struct interrupt_frame));
        task->frame->rip = USER_CODE;
        task->frame->cs = 0x23;
        task->frame->ss = 0x1b;
        task->frame->rsp = stack_base + 2 * PAGE_SIZE - 8;
        task->frame->rflags = 0x202;
    }
#if MINILINUX_ATTACK != 0
    counterexample_user_prepare(task_get(0)->space, task_get(0)->data_page);
#endif
}

bool user_range(const struct task *task, uint64_t address, size_t length, bool writing) {
    if (length > COPY_LIMIT || address >= (UINT64_C(1) << 47) ||
        length > (UINT64_C(1) << 47) - address) { return false; }
    size_t checked = 0;
    while (checked < length) {
        uint64_t physical = 0;
        unsigned flags = 0;
        if (!vm_lookup(task->space, address + checked, &physical, &flags) ||
            !(flags & VM_USER) || (writing && !(flags & VM_WRITE))) { return false; }
        size_t step = PAGE_SIZE - (address + checked) % PAGE_SIZE;
        if (step > length - checked) { step = length - checked; }
        checked += step;
    }
    return true;
}

struct interrupt_frame *user_fault(struct interrupt_frame *frame, uint64_t address) {
    struct task *task = task_current();
    serial_write(frame->vector == 14 ? "user page fault: task=" : "user exception: task=");
    serial_write_number(task->id);
    serial_write(" vector="); serial_write_number(frame->vector);
    serial_write(" error="); serial_write_hex(frame->error);
    serial_write(" CS="); serial_write_hex(frame->cs);
    if (frame->vector == 14) { serial_write(" CR2="); serial_write_hex(address); }
    serial_write(" RIP="); serial_write_hex(frame->rip); serial_write("\n");
    uint64_t expected_address = KERNEL_PROBE, expected_error = 5, expected_vector = 14;
#if MINILINUX_LEVEL >= 6
    switch (MINILINUX_PROBE) {
    case 1: break;
    case 2: expected_address = USER_CODE; expected_error = 7; break;
    case 3: expected_address = USER_DATA; expected_error = 21; break;
    case 4: expected_address = 0x900000; expected_error = 4; break;
    case 5: expected_vector = 6; expected_address = 0; expected_error = 0; break;
    case 6: expected_vector = 13; expected_address = 0; expected_error = 0; break;
    case 7: expected_vector = 0; expected_address = 0; expected_error = 0; break;
    }
#endif
    if (task->id != 0 || !task->reported || frame->cs != 0x23 ||
        frame->vector != expected_vector || address != expected_address || frame->error != expected_error) {
        panic("unexpected user fault");
    }
    task->fault_seen = true;
    return task_exit(frame, 0);
}

__attribute__((noreturn)) void user_complete(void) {
    for (unsigned i = 0; i < 2; i++) {
        struct task *task = task_get(i);
        if (!task->reported || !task->user_seen || task->slices < 2 ||
            task->rejected_pointers < 3 || task->rejected_calls < 1) {
            panic("user boundary observations are incomplete");
        }
#if MINILINUX_LEVEL >= 5
        if (task->files_opened != 3 || task->bytes_read != 18 || task->eof_reads < 2 ||
            task->file_errors < 4 || task->files[0].used || task->files[1].used) {
            panic("file observations are incomplete");
        }
#endif
    }
    if (!task_get(0)->fault_seen || task_get(1)->fault_seen) { panic("fault isolation differs"); }
#if MINILINUX_LEVEL >= 6
    if (spaces[0].root == spaces[1].root || task_get(0)->data_page == task_get(1)->data_page) {
        panic("processes share a root or data page");
    }
    for (unsigned i = 0; i < 2; i++) {
        const uint64_t *data = page_data(task_get(i)->data_page);
        if (data[0] != UINT64_C(0xa110) + i || data[1] == 0) {
            panic("physical process readback differs");
        }
        serial_write("M6 isolated VA "); serial_write_hex(USER_DATA);
        serial_write(" root="); serial_write_hex(spaces[i].root);
        serial_write(" PA="); serial_write_hex(task_get(i)->data_page);
        serial_write(" marker="); serial_write_hex(data[0]);
        serial_write(" counter="); serial_write_number(data[1]); serial_write("\n");
    }
#endif
    vm_switch(boot_root);
    if (!vm_destroy(&spaces[0])) { panic("user root cleanup failed"); }
#if MINILINUX_LEVEL >= 6
    if (!vm_destroy(&spaces[1])) { panic("second user root cleanup failed"); }
#endif
    for (unsigned i = 0; i < owned_count; i++) {
        if (!page_free(owned_pages[i])) { panic("user page cleanup failed"); }
    }
    struct page_info info;
    page_get_info(&info);
    if (info.free_count != initial_free) { panic("user experiment leaked pages"); }
#if MINILINUX_LEVEL == 4
    serial_write("MiniLinux M4: user boundary checks passed\n");
    panic("M4 reached its intentional stop");
#elif MINILINUX_LEVEL == 5
    serial_write("MiniLinux M5: file byte checks passed\n");
    panic("M5 reached its intentional stop");
#else
    serial_write("MiniLinux M6: process isolation checks passed\n");
    panic("M6 reached its intentional stop");
#endif
}
