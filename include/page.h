#ifndef MINILINUX_PAGE_H
#define MINILINUX_PAGE_H

#include <stdbool.h>
#include <stdint.h>

enum { PAGE_SIZE = 4096 };

struct limine_memmap_response;

struct page_info {
    uint64_t frame_count;
    uint64_t free_count;
    uint64_t metadata_base;
    uint64_t metadata_pages;
};

bool page_init(const struct limine_memmap_response *map, uint64_t hhdm_offset);
/* Addresses returned here are physical, not C pointers. Failure leaves *address intact. */
bool page_alloc(uint64_t *address);
/* Only a page currently allocated by this allocator may be freed. */
bool page_free(uint64_t address);
/* Borrow Limine's HHDM mapping to access a currently allocated page. */
void *page_data(uint64_t address);
void page_get_info(struct page_info *info);
void page_selftest(const struct limine_memmap_response *map, uint64_t hhdm_offset);

#endif
