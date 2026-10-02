#ifndef MINILINUX_VM_H
#define MINILINUX_VM_H

#include <stdbool.h>
#include <stdint.h>

enum { VM_WRITE = 1, VM_USER = 2, VM_EXEC = 4, VM_TABLE_LIMIT = 16 };

/* Own only the page tables. The caller owns the pages mapped into them. */
struct address_space {
    uint64_t root;
    uint64_t tables[VM_TABLE_LIMIT];
    unsigned table_count;
};

void vm_init(uint64_t hhdm_offset);
uint64_t vm_current_root(void);
void vm_switch(uint64_t root);
bool vm_create(struct address_space *space);
bool vm_map(struct address_space *space, uint64_t virtual_address,
            uint64_t physical_address, unsigned flags);
/* Return effective permissions across all four levels, not only leaf flags. */
bool vm_lookup(const struct address_space *space, uint64_t virtual_address,
               uint64_t *physical_address, unsigned *flags);
bool vm_unmap(struct address_space *space, uint64_t virtual_address);
bool vm_destroy(struct address_space *space);
void vm_selftest(void);

#endif
