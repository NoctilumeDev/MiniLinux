#include <stddef.h>
#include "console.h"
#include "page.h"
#include "vm.h"

#define PTE_PRESENT UINT64_C(1)
#define PTE_WRITE UINT64_C(2)
#define PTE_USER UINT64_C(4)
#define PTE_LARGE UINT64_C(128)
#define PTE_NX (UINT64_C(1) << 63)
#define PTE_ADDRESS UINT64_C(0x000ffffffffff000)

static uint64_t direct_offset;
static uint64_t boot_root;

static uint64_t *table_at(uint64_t physical) {
    return (uint64_t *)(uintptr_t)(direct_offset + physical);
}

uint64_t vm_current_root(void) {
    uint64_t root;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(root));
    return root & PTE_ADDRESS;
}

void vm_switch(uint64_t root) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(root) : "memory");
}

void vm_init(uint64_t hhdm_offset) {
    uint64_t cr4, cr0;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    if (cr4 & (UINT64_C(1) << 12)) { panic("M2 requires four-level paging"); }
    direct_offset = hhdm_offset;
    boot_root = vm_current_root();
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xc0000080));
    low |= 1u << 11; /* EFER.NXE: instruction fetch can be denied per page. */
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(0xc0000080));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= UINT64_C(1) << 16; /* CR0.WP: kernel writes obey read-only pages too. */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

static bool new_table(struct address_space *space, uint64_t *physical) {
    if (space->table_count == VM_TABLE_LIMIT || !page_alloc(physical)) { return false; }
    uint64_t *table = table_at(*physical);
    for (unsigned i = 0; i < 512; i++) { table[i] = 0; }
    space->tables[space->table_count++] = *physical;
    return true;
}

bool vm_create(struct address_space *space) {
    if (space == NULL || space->root != 0 || space->table_count != 0) { return false; }
    if (!new_table(space, &space->root)) { return false; }
    uint64_t *root = table_at(space->root);
    uint64_t *boot = table_at(boot_root);
    for (unsigned i = 256; i < 512; i++) {
        root[i] = boot[i] & ~PTE_USER;
    }
    return true;
}

static unsigned entry_flags(uint64_t entry) {
    return ((entry & PTE_WRITE) ? VM_WRITE : 0) |
           ((entry & PTE_USER) ? VM_USER : 0) | ((entry & PTE_NX) ? 0 : VM_EXEC);
}

/* Every level can restrict access. Walk only the lower half we own. */
static uint64_t *leaf_entry(const struct address_space *space, uint64_t address,
                            unsigned *permissions) {
    if (space == NULL || space->root == 0 || address >= (UINT64_C(1) << 47)) {
        return NULL;
    }
    uint64_t *table = table_at(space->root);
    for (int shift = 39; shift >= 21; shift -= 9) {
        uint64_t entry = table[(address >> shift) & 511];
        if (!(entry & PTE_PRESENT) || (entry & PTE_LARGE)) { return NULL; }
        if (permissions != NULL) { *permissions &= entry_flags(entry); }
        table = table_at(entry & PTE_ADDRESS);
    }
    return &table[(address >> 12) & 511];
}

bool vm_map(struct address_space *space, uint64_t address, uint64_t physical,
            unsigned flags) {
    if (space == NULL || space->root == 0 || address == 0 ||
        address >= (UINT64_C(1) << 47) || address % PAGE_SIZE != 0 ||
        physical % PAGE_SIZE != 0 || page_data(physical) == NULL ||
        (flags & ~(VM_WRITE | VM_USER | VM_EXEC)) != 0) { return false; }
    uint64_t *existing = leaf_entry(space, address, NULL);
    if (existing != NULL && (*existing & PTE_PRESENT)) { return false; }
    uint64_t *table = table_at(space->root);
    for (int shift = 39; shift >= 21; shift -= 9) {
        unsigned index = (address >> shift) & 511;
        if (!(table[index] & PTE_PRESENT)) {
            uint64_t next = 0;
            if (!new_table(space, &next)) { return false; }
            /* New parents permit traversal; existing parents can restrict it. */
            table[index] = next | PTE_PRESENT | PTE_WRITE | PTE_USER;
        }
        if (table[index] & PTE_LARGE) { return false; }
        table = table_at(table[index] & PTE_ADDRESS);
    }
    uint64_t entry = physical | PTE_PRESENT;
    if (flags & VM_WRITE) { entry |= PTE_WRITE; }
    if (flags & VM_USER) { entry |= PTE_USER; }
    if (!(flags & VM_EXEC)) { entry |= PTE_NX; }
    table[(address >> 12) & 511] = entry;
    return true;
}

bool vm_lookup(const struct address_space *space, uint64_t address,
               uint64_t *physical, unsigned *flags) {
    unsigned permissions = VM_WRITE | VM_USER | VM_EXEC;
    uint64_t *leaf = leaf_entry(space, address, &permissions);
    if (leaf == NULL || !(*leaf & PTE_PRESENT)) { return false; }
    *physical = (*leaf & PTE_ADDRESS) + address % PAGE_SIZE;
    *flags = permissions & entry_flags(*leaf);
    return true;
}

bool vm_unmap(struct address_space *space, uint64_t address) {
    if (address % PAGE_SIZE != 0) { return false; }
    uint64_t *leaf = leaf_entry(space, address, NULL);
    if (leaf == NULL || !(*leaf & PTE_PRESENT)) { return false; }
    *leaf = 0;
    if (space->root == vm_current_root()) {
        __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
    }
    return true;
}

bool vm_destroy(struct address_space *space) {
    if (space == NULL || space->root == 0 || space->root == vm_current_root()) {
        return false;
    }
    for (unsigned i = space->table_count; i > 0; i--) {
        if (!page_free(space->tables[i - 1])) { panic("page table ownership differs"); }
    }
    space->root = 0;
    space->table_count = 0;
    return true;
}
