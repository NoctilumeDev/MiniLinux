#include <stddef.h>
#include "limine.h"
#include "page.h"

enum page_state { PAGE_UNMANAGED, PAGE_FREE, PAGE_ALLOCATED };

/* One owner for the page states; callers cannot copy the allocator. */
static struct {
    uint8_t *states;
    uint64_t frame_count;
    uint64_t free_count;
    uint64_t next_frame;
    uint64_t hhdm_offset;
    uint64_t metadata_base;
    uint64_t metadata_pages;
} pages;

bool page_init(const struct limine_memmap_response *map, uint64_t hhdm_offset) {
    if (pages.states != NULL || map == NULL || map->entries == NULL) {
        return false;
    }

    uint64_t highest = 0;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry == NULL) {
            return false;
        }
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        if (entry->base % PAGE_SIZE != 0 || entry->length % PAGE_SIZE != 0 ||
            entry->length > UINT64_MAX - entry->base) {
            return false;
        }
        uint64_t end = entry->base + entry->length;
        if (end > highest) {
            highest = end;
        }
    }
    if (highest == 0 || highest - 1 > UINT64_MAX - hhdm_offset) {
        return false;
    }

    uint64_t frame_count = highest / PAGE_SIZE;
    uint64_t metadata_pages = (frame_count + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t metadata_bytes = metadata_pages * PAGE_SIZE;
    uint64_t metadata_base = 0;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        uint64_t base = entry->base == 0 ? PAGE_SIZE : entry->base;
        uint64_t end = entry->base + entry->length;
        if (base <= end && metadata_bytes <= end - base) {
            metadata_base = base;
            break;
        }
    }
    if (metadata_base == 0) {
        return false;
    }

    pages.states = (uint8_t *)(uintptr_t)(hhdm_offset + metadata_base);
    pages.frame_count = frame_count;
    pages.metadata_base = metadata_base;
    pages.metadata_pages = metadata_pages;
    pages.hhdm_offset = hhdm_offset;
    for (uint64_t frame = 0; frame < frame_count; frame++) {
        pages.states[frame] = PAGE_UNMANAGED;
    }

    uint64_t metadata_end = metadata_base + metadata_bytes;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        uint64_t end = entry->base + entry->length;
        for (uint64_t address = entry->base; address < end; address += PAGE_SIZE) {
            if (address == 0 || (address >= metadata_base && address < metadata_end)) {
                continue;
            }
            pages.states[address / PAGE_SIZE] = PAGE_FREE;
            pages.free_count++;
        }
    }
    return true;
}

bool page_alloc(uint64_t *address) {
    if (address == NULL || pages.states == NULL || pages.free_count == 0) {
        return false;
    }
    uint64_t frame = pages.next_frame;
    for (uint64_t checked = 0; checked < pages.frame_count; checked++) {
        if (pages.states[frame] == PAGE_FREE) {
            pages.states[frame] = PAGE_ALLOCATED;
            pages.free_count--;
            pages.next_frame = frame + 1 == pages.frame_count ? 0 : frame + 1;
            *address = frame * PAGE_SIZE;
            return true;
        }
        frame++;
        if (frame == pages.frame_count) {
            frame = 0;
        }
    }
    return false;
}

bool page_free(uint64_t address) {
    if (pages.states == NULL || address % PAGE_SIZE != 0 ||
        address / PAGE_SIZE >= pages.frame_count ||
        pages.states[address / PAGE_SIZE] != PAGE_ALLOCATED) {
        return false;
    }
    pages.states[address / PAGE_SIZE] = PAGE_FREE;
    pages.free_count++;
    return true;
}

void *page_data(uint64_t address) {
    if (pages.states == NULL || address % PAGE_SIZE != 0 ||
        address / PAGE_SIZE >= pages.frame_count ||
        pages.states[address / PAGE_SIZE] != PAGE_ALLOCATED) {
        return NULL;
    }
    return (void *)(uintptr_t)(pages.hhdm_offset + address);
}

void page_get_info(struct page_info *info) {
    info->frame_count = pages.frame_count;
    info->free_count = pages.free_count;
    info->metadata_base = pages.metadata_base;
    info->metadata_pages = pages.metadata_pages;
}
