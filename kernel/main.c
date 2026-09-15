#include <stdint.h>
#include "limine.h"

/* Limine looks for these values in the loaded ELF. */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

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

static void serial_write(const char *message) {
    for (const char *p = message; *p != '\0'; p++) {
        if (*p == '\n') {
            serial_putc('\r');
        }
        serial_putc(*p);
    }
}

__attribute__((noreturn))
static void panic(const char *message) {
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
    serial_write("MiniLinux M0: entered kernel_main\n");
    panic("M0 reached its intentional stop");
}
