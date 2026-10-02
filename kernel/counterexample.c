#include "console.h"
#include "counterexample.h"
#include "page.h"

#define ENTRY_ADDRESS UINT64_C(0x000ffffffffff000)

#if MINILINUX_ATTACK <= 3
static void require(bool condition, const char *message) {
    if (!condition) { panic(message); }
}
#endif

/* Test fixture: mutate an inactive table through its owned physical pages. */
#if MINILINUX_ATTACK <= 2
static uint64_t *parent_entry(struct address_space *space, uint64_t address, int level) {
    uint64_t *table = page_data(space->root);
    for (int shift = 39; shift >= 21; shift -= 9) {
        unsigned index = (address >> shift) & 511;
        if (shift == level) { return &table[index]; }
        table = page_data(table[index] & ENTRY_ADDRESS);
    }
    panic("counterexample parent level differs");
}
#endif

#if MINILINUX_ATTACK == 3
static void exhausted_map(unsigned spare) {
    struct page_info before, after;
    page_get_info(&before);
    struct address_space space = {0};
    uint64_t data = 0, head = 0, next = 0;
    require(vm_create(&space) && page_alloc(&data), "OOM fixture allocation failed");
    /* Every held page stores the next address, so no separate big array is needed. */
    while (page_alloc(&next)) {
        *(uint64_t *)page_data(next) = head;
        head = next;
    }
    for (unsigned i = 0; i < spare; i++) {
        require(head != 0, "OOM fixture has too few held pages");
        uint64_t released = head;
        head = *(uint64_t *)page_data(head);
        require(page_free(released), "OOM fixture release failed");
    }
    require(!vm_map(&space, 0x400000, data, VM_USER | VM_WRITE), "OOM map unexpectedly succeeded");
    uint64_t physical = UINT64_MAX;
    unsigned flags = 99;
    require(!vm_lookup(&space, 0x400000, &physical, &flags) && physical == UINT64_MAX && flags == 99,
            "OOM failure installed a leaf or changed output");
    require(space.table_count == 1 + spare, "partial tables have no recorded owner");
    require(vm_destroy(&space) && page_free(data), "OOM cleanup failed");
    while (head != 0) {
        uint64_t released = head;
        head = *(uint64_t *)page_data(head);
        require(page_free(released), "OOM held page cleanup failed");
    }
    page_get_info(&after);
    require(after.free_count == before.free_count, "OOM fixture leaked pages");
    require(vm_create(&space) && page_alloc(&data) && vm_map(&space, 0x400000, data, VM_WRITE),
            "normal mapping cannot resume after OOM");
    require(vm_destroy(&space) && page_free(data), "OOM recovery cleanup failed");
    page_get_info(&after);
    require(after.free_count == before.free_count, "OOM recovery leaked pages");
    serial_write("counterexample: OOM with spare pages="); serial_write_number(spare);
    serial_write(" rejected; partial ownership and recovery checked\n");
}
#endif

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
        require(vm_lookup(&space, 0x400000 + 17, &physical, &flags) && physical == data + 17 &&
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
#elif MINILINUX_ATTACK == 3
    for (unsigned spare = 0; spare < 3; spare++) { exhausted_map(spare); }
    struct page_info before, after;
    page_get_info(&before);
    struct address_space space = {0};
    uint64_t data = 0, physical = 0;
    unsigned flags = 0;
    require(vm_create(&space) && page_alloc(&data), "table limit fixture allocation failed");
    for (unsigned i = 0; i < 5; i++) {
        require(vm_map(&space, ((uint64_t)i << 39) + 0x400000, data, VM_USER | VM_WRITE),
                "table limit fixture map failed too early");
    }
    page_get_info(&after);
    uint64_t remaining = after.free_count;
    require(space.table_count == VM_TABLE_LIMIT &&
            !vm_map(&space, (UINT64_C(5) << 39) + 0x400000, data, VM_WRITE) &&
            !vm_lookup(&space, (UINT64_C(5) << 39) + 0x400000, &physical, &flags),
            "table limit did not reject the missing branch");
    page_get_info(&after);
    require(after.free_count == remaining, "table limit consumed a page");
    require(vm_map(&space, 0x401000, data, VM_WRITE), "table limit blocked an existing branch");
    require(vm_destroy(&space) && page_free(data), "table limit cleanup failed");
    page_get_info(&after);
    require(after.free_count == before.free_count, "table limit leaked pages");
    serial_write("counterexample: table limit rejected new branch; existing branch and cleanup passed\n");
#endif
}
