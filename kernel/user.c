#include "abi.h"
#include "bytes.h"
#include "console.h"
#include "page.h"
#include "user.h"

extern const unsigned char user_image[];
extern const uint64_t user_image_size;
static struct address_space shared_space;
static uint64_t owned_pages[7]; /* Two code, one data, four stack pages. */
static unsigned owned_count;
static uint64_t boot_root, initial_free;

static uint64_t map_page(uint64_t address, unsigned flags) {
    uint64_t physical = 0;
    if (!page_alloc(&physical)) { panic("user data allocation failed"); }
    owned_pages[owned_count++] = physical;
    memset(page_data(physical), 0, PAGE_SIZE);
    if (!vm_map(&shared_space, address, physical, flags)) { panic("user mapping failed"); }
    return physical;
}

void user_prepare(void) {
    struct page_info info;
    page_get_info(&info);
    initial_free = info.free_count;
    boot_root = vm_current_root();
    if (user_image_size == 0 || user_image_size > 2 * PAGE_SIZE || !vm_create(&shared_space)) {
        panic("user image or address space is invalid");
    }
    for (unsigned i = 0; i < 2; i++) {
        uint64_t code = map_page(USER_CODE + i * PAGE_SIZE, VM_USER | VM_EXEC);
        uint64_t offset = i * PAGE_SIZE;
        if (offset < user_image_size) {
            uint64_t length = user_image_size - offset;
            if (length > PAGE_SIZE) { length = PAGE_SIZE; }
            memcpy(page_data(code), &user_image[offset], length);
        }
    }
    map_page(USER_DATA, VM_USER | VM_WRITE);
    for (unsigned i = 0; i < 2; i++) {
        struct task *task = task_get(i);
        task->space = &shared_space;
        uint64_t stack_base = USER_STACK + i * UINT64_C(0x20000);
        map_page(stack_base, VM_USER | VM_WRITE);
        map_page(stack_base + PAGE_SIZE, VM_USER | VM_WRITE);
        uint64_t top = (uint64_t)(uintptr_t)&task->stack[8192];
        task->frame = (struct interrupt_frame *)(uintptr_t)(top - sizeof(struct interrupt_frame));
        task->frame->rip = USER_CODE;
        task->frame->cs = 0x23;
        task->frame->ss = 0x1b;
        task->frame->rsp = stack_base + 2 * PAGE_SIZE - 8;
        task->frame->rflags = 0x202;
    }
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
    serial_write("user page fault: task="); serial_write_number(task->id);
    serial_write(" vector=14 error="); serial_write_hex(frame->error);
    serial_write(" CR2="); serial_write_hex(address);
    serial_write(" RIP="); serial_write_hex(frame->rip); serial_write("\n");
    if (task->id != 0 || !task->reported || frame->cs != 0x23 ||
        address != KERNEL_PROBE || frame->error != 5) { panic("unexpected user fault"); }
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
    vm_switch(boot_root);
    if (!vm_destroy(&shared_space)) { panic("user root cleanup failed"); }
    for (unsigned i = 0; i < owned_count; i++) {
        if (!page_free(owned_pages[i])) { panic("user page cleanup failed"); }
    }
    struct page_info info;
    page_get_info(&info);
    if (info.free_count != initial_free) { panic("user experiment leaked pages"); }
#if MINILINUX_LEVEL == 4
    serial_write("MiniLinux M4: user boundary checks passed\n");
    panic("M4 reached its intentional stop");
#else
    serial_write("MiniLinux M5: file byte checks passed\n");
    panic("M5 reached its intentional stop");
#endif
}
