#include "console.h"
#include "counterexample.h"
#include "page.h"

#define ENTRY_ADDRESS UINT64_C(0x000ffffffffff000)

static void require(bool condition, const char *message) {
    if (!condition) { panic(message); }
}

/* Test fixture: mutate an inactive table through its owned physical pages. */
static uint64_t *parent_entry(struct address_space *space, uint64_t address, int level) {
    uint64_t *table = page_data(space->root);
    for (int shift = 39; shift >= 21; shift -= 9) {
        unsigned index = (address >> shift) & 511;
        if (shift == level) { return &table[index]; }
        table = page_data(table[index] & ENTRY_ADDRESS);
    }
    panic("counterexample parent level differs");
}

void counterexample_user_prepare(struct address_space *space, uint64_t data_page) {
#if MINILINUX_ATTACK == 1
    /* A separate PD entry leaves code, data and stack normally accessible. */
    require(vm_map(space, 0xa00000, data_page, VM_USER | VM_WRITE), "parent fixture map failed");
    uint64_t *entry = parent_entry(space, 0xa00000, 21);
    *entry &= ~UINT64_C(2);
    serial_write("counterexample: read-only parent with writable leaf installed\n");
#else
    (void)space;
    (void)data_page;
#endif
}

void counterexample_selftest(void) {
#if MINILINUX_ATTACK == 2
    struct page_info before, after;
    page_get_info(&before);
    struct address_space space = {0};
    uint64_t data = 0, physical = 0;
    unsigned flags = 0;
    require(vm_create(&space) && page_alloc(&data), "permission fixture allocation failed");
    require(vm_map(&space, 0x400000, data, VM_USER | VM_WRITE | VM_EXEC), "permission fixture map failed");
    for (int level = 39; level >= 21; level -= 9) {
        uint64_t *entry = parent_entry(&space, 0x400000, level);
        uint64_t saved = *entry;
        *entry = saved & ~UINT64_C(2);
        require(vm_lookup(&space, 0x400017, &physical, &flags) && physical == data + 17 &&
                flags == (VM_USER | VM_EXEC), "counterexample: ancestor write restriction ignored");
        *entry = saved & ~UINT64_C(4);
        require(vm_lookup(&space, 0x400000, &physical, &flags) && flags == (VM_WRITE | VM_EXEC),
                "counterexample: ancestor user restriction ignored");
        *entry = saved | (UINT64_C(1) << 63);
        require(vm_lookup(&space, 0x400000, &physical, &flags) && flags == (VM_WRITE | VM_USER),
                "counterexample: ancestor execute restriction ignored");
        *entry = saved & ~UINT64_C(1);
        physical = UINT64_MAX;
        flags = 99;
        require(!vm_lookup(&space, 0x400000, &physical, &flags) && physical == UINT64_MAX && flags == 99,
                "counterexample: absent parent changed lookup output");
        *entry = saved;
    }
    require(vm_destroy(&space) && page_free(data), "permission fixture cleanup failed");
    page_get_info(&after);
    require(after.free_count == before.free_count, "permission fixture leaked pages");
    serial_write("counterexample: all ancestor permissions checked; pages restored\n");
#endif
}
