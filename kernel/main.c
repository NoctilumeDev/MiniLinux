#include <stdint.h>
#include "console.h"
#include "limine.h"
#ifdef MINILINUX_M1
#include "page.h"
#endif

/* Limine looks for these values in the loaded ELF. */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

#ifdef MINILINUX_M1
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};
#endif

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

enum { COM1 = 0x3f8 };

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable UART interrupts. */
    outb(COM1 + 3, 0x80); /* Set the baud-rate divisor. */
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 data bits, no parity, one stop bit. */
    outb(COM1 + 2, 0x07); /* Enable and clear FIFO. */
    outb(COM1 + 4, 0x0b); /* Assert RTS and DTR. */
}

static void serial_putc(char character) {
    while ((inb(COM1 + 5) & 0x20) == 0) { }
    outb(COM1, (uint8_t)character);
}

void serial_write(const char *message) {
    for (const char *p = message; *p != '\0'; p++) {
        if (*p == '\n') {
            serial_putc('\r');
        }
        serial_putc(*p);
    }
}

void serial_write_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> shift) & 0xf]);
    }
}

void serial_write_number(uint64_t value) {
    char buffer[21];
    unsigned index = 20;
    buffer[index] = '\0';
    do {
        buffer[--index] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    serial_write(&buffer[index]);
}

__attribute__((noreturn))
void panic(const char *message) {
    serial_write("MiniLinux PANIC: ");
    serial_write(message);
    serial_write("\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

__attribute__((noreturn))
void kernel_main(void) {
    serial_init();
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        panic("Limine base revision 6 is unavailable");
    }
#ifdef MINILINUX_M1
    serial_write("MiniLinux M1: entered kernel_main\n");
    if (memory_map_request.response == 0 || hhdm_request.response == 0) {
        panic("Limine memory map or HHDM response is unavailable");
    }
    if (!page_init(memory_map_request.response, hhdm_request.response->offset)) {
        panic("physical page initialization failed");
    }
    page_selftest(memory_map_request.response, hhdm_request.response->offset);
    panic("M1 reached its intentional stop");
#else
    serial_write("MiniLinux M0: entered kernel_main\n");
    panic("M0 reached its intentional stop");
#endif
}
