#include <stddef.h>
#include "console.h"
#include "limine.h"
#include "page.h"

static void require(bool condition, const char *message) {
    if (!condition) {
        panic(message);
    }
}

static uint64_t free_count(void) {
    struct page_info info;
    page_get_info(&info);
    return info.free_count;
}

static bool usable_page(const struct limine_memmap_response *map, uint64_t address) {
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && address >= entry->base &&
            address < entry->base + entry->length) {
            return true;
        }
    }
    return false;
}

static void reject_free(uint64_t address, const char *reason) {
    uint64_t before = free_count();
    require(!page_free(address), reason);
    require(free_count() == before, "rejected free changed the free count");
    serial_write("M1 rejected free: ");
    serial_write(reason);
    serial_write(" at ");
    serial_write_hex(address);
    serial_write("\n");
}

void page_selftest(const struct limine_memmap_response *map, uint64_t hhdm_offset) {
    struct page_info info;
    page_get_info(&info);
    uint64_t initial_free = info.free_count;
    require(initial_free >= 2, "fewer than two free pages");

    serial_write("M1 HHDM offset: ");
    serial_write_hex(hhdm_offset);
    serial_write("\nM1 tracked frames: ");
    serial_write_number(info.frame_count);
    serial_write("; free pages: ");
    serial_write_number(initial_free);
    serial_write("\nM1 metadata physical: ");
    serial_write_hex(info.metadata_base);
    serial_write("; reserved pages: ");
    serial_write_number(info.metadata_pages);
    serial_write("\n");
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        serial_write("M1 map base=");
        serial_write_hex(entry->base);
        serial_write(" length=");
        serial_write_hex(entry->length);
        serial_write(" type=");
        serial_write_number(entry->type);
        serial_write("\n");
    }

    uint64_t first = 0;
    uint64_t second = 0;
    require(page_alloc(&first), "first page allocation failed");
    require(page_alloc(&second), "second page allocation failed");
    require(first != second && first % PAGE_SIZE == 0 && second % PAGE_SIZE == 0,
            "allocated pages are duplicate or unaligned");
    require(usable_page(map, first) && usable_page(map, second), "allocated page is not usable");
    require(free_count() == initial_free - 2, "initial allocation count differs");
    uint8_t *a = page_data(first);
    uint8_t *b = page_data(second);
    require(a != NULL && b != NULL, "allocated page has no HHDM pointer");
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        a[i] = 0x35;
        b[i] = 0xca;
    }
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        require(a[i] == 0x35 && b[i] == 0xca, "distinct page readback differs");
    }
    serial_write("M1 distinct physical pages: ");
    serial_write_hex(first);
    serial_write(" / ");
    serial_write_hex(second);
    serial_write("; 4096-byte readback passed\n");

    reject_free(0, "zero page");
    reject_free(info.metadata_base, "allocator metadata");
    reject_free(first + 1, "unaligned address");
    reject_free(info.frame_count * PAGE_SIZE, "outside managed range");
    bool reclaimable_seen = false;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE && entry->length >= PAGE_SIZE) {
            reject_free(entry->base, "bootloader still owns this page");
            reclaimable_seen = true;
            break;
        }
    }
    require(reclaimable_seen, "no bootloader-reclaimable page to check");
    require(!page_init(map, hhdm_offset), "allocator accepted a second initialization");
    require(free_count() == initial_free - 2, "second initialization changed page state");

    uint64_t allocated = 2;
    uint64_t last = second;
    uint64_t address = 0;
    while (page_alloc(&address)) {
        require(address > last, "allocation order repeated or moved backwards");
        require(usable_page(map, address), "exhaustion reached a non-usable page");
        require(address < info.metadata_base ||
                address >= info.metadata_base + info.metadata_pages * PAGE_SIZE,
                "allocator handed out its own metadata");
        allocated++;
        require(allocated <= initial_free, "allocator exceeded its initial free count");
        last = address;
    }
    require(allocated == initial_free && free_count() == 0, "exhaustion count differs");
    address = UINT64_MAX;
    require(!page_alloc(&address) && address == UINT64_MAX, "exhausted allocation changed output");

    require(page_free(first) && free_count() == 1, "release after exhaustion failed");
    require(page_data(first) == NULL, "released page still has an allocated-page pointer");
    reject_free(first, "double free");
    uint64_t reused = 0;
    require(page_alloc(&reused) && reused == first && free_count() == 0,
            "the only released page was not reusable");
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        require(a[i] == 0x35 && b[i] == 0xca, "page content changed during allocator operations");
    }
    serial_write("M1 exhaustion: ");
    serial_write_number(allocated);
    serial_write(" distinct usable pages; released page reused\n");

    /* This boot experiment owns every allocated page; no other consumer exists yet. */
    uint64_t returned = 0;
    for (uint64_t frame = 0; frame < info.frame_count; frame++) {
        if (page_free(frame * PAGE_SIZE)) {
            returned++;
        }
    }
    require(returned == initial_free && free_count() == initial_free,
            "returning experiment pages did not restore the free count");
    reject_free(first, "already returned experiment page");
    require(page_alloc(&address) && page_free(address), "allocator cannot restart after return");
    require(free_count() == initial_free, "final free count differs");
    serial_write("M1 restored free pages: ");
    serial_write_number(free_count());
    serial_write("\nMiniLinux M1: physical page checks passed\n");
}
