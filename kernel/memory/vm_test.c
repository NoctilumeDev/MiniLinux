#include "console.h"
#include "page.h"
#include "vm.h"

static void require(bool condition, const char *message) {
    if (!condition) { panic(message); }
}

void vm_selftest(void) {
    struct page_info before, after;
    page_get_info(&before);
    struct address_space space = {0};
    uint64_t first = 0, second = 0, result = 0;
    unsigned flags = 0;
    uint64_t boot = vm_current_root();
    require(vm_create(&space), "M2 root allocation failed");
    require(page_alloc(&first), "M2 first data page failed");
    require(page_alloc(&second), "M2 second data page failed");
    const uint64_t address = 0x400000, alias = 0x401000;
    require(vm_map(&space, address, first, VM_WRITE), "M2 mapping failed");
    require(vm_map(&space, alias, first, VM_WRITE), "M2 alias failed");
    require(!vm_map(&space, address, second, VM_WRITE), "M2 overwrote a mapping");
    require(!vm_map(&space, alias + 1, second, VM_WRITE), "M2 accepted unaligned VA");
    require(!vm_map(&space, 0x402000, second + 1, VM_WRITE), "M2 accepted unaligned PA");
    require(!vm_map(&space, UINT64_C(1) << 47, second, VM_WRITE), "M2 accepted upper VA");
    require(vm_lookup(&space, address + 17, &result, &flags) && result == first + 17 &&
            flags == VM_WRITE, "M2 page walk differs");
    vm_switch(space.root);
    require(vm_current_root() == space.root && !vm_destroy(&space), "M2 active root differs");
    volatile uint8_t *a = (volatile uint8_t *)(uintptr_t)address;
    volatile uint8_t *b = (volatile uint8_t *)(uintptr_t)alias;
    uint8_t *physical = page_data(first);
    for (unsigned i = 0; i < PAGE_SIZE; i++) { a[i] = (uint8_t)i; }
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        require(b[i] == (uint8_t)i && physical[i] == (uint8_t)i, "M2 alias readback differs");
    }
    require(vm_unmap(&space, alias), "M2 unmap failed");
    require(!vm_lookup(&space, alias, &result, &flags) && !vm_unmap(&space, alias),
            "M2 removed mapping still present");
    uint8_t *replacement = page_data(second);
    for (unsigned i = 0; i < PAGE_SIZE; i++) { replacement[i] = 0x5a; }
    require(vm_map(&space, alias, second, VM_WRITE), "M2 remap failed");
    for (unsigned i = 0; i < PAGE_SIZE; i++) {
        require(b[i] == 0x5a && a[i] == (uint8_t)i, "M2 stale translation remains");
    }
    serial_write("M2 active CR3: "); serial_write_hex(vm_current_root());
    serial_write("; VA "); serial_write_hex(address);
    serial_write(" -> PA "); serial_write_hex(first);
    serial_write("; alias and HHDM readback passed\n");
    vm_switch(boot);
    require(vm_destroy(&space) && page_free(first) && page_free(second), "M2 cleanup failed");
    page_get_info(&after);
    require(after.free_count == before.free_count, "M2 leaked experiment pages");
    serial_write("MiniLinux M2: address mapping checks passed\n");
}
