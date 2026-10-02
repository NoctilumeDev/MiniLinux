#include "abi.h"
#include "page.h"
#include "user.h"

/* Both the fixed experiments and the shell obey the same copy boundary. */
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
