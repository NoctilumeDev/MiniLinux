#include <stdint.h>
#include "console.h"
#if MINILINUX_ATTACK != 0
#include "counterexample.h"
#endif
#include "limine.h"
#if MINILINUX_LEVEL >= 1
#include "page.h"
#endif
#if MINILINUX_LEVEL >= 2
#include "vm.h"
#endif
#if MINILINUX_LEVEL >= 3
#include "cpu.h"
#include "task.h"
#endif

/* Limine looks for these values in the loaded ELF. */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

#if MINILINUX_LEVEL >= 1
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

static void uart_init(uint16_t port) {
    outb(port + 1, 0x00); /* Disable UART interrupts. */
    outb(port + 3, 0x80); /* Set the baud-rate divisor. */
    outb(port + 0, 0x01);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03); /* 8 data bits, no parity, one stop bit. */
    outb(port + 2, 0x07); /* Enable and clear FIFO. */
    outb(port + 4, 0x0b); /* Assert RTS and DTR. */
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

void serial_write_bytes(const char *bytes, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (bytes[i] == '\n') { serial_putc('\r'); }
        serial_putc(bytes[i]);
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

#if MINILINUX_LEVEL == 7
/* Keep observations on COM2 so they never become shell input or output. */
static void trace_char(char value) {
    while ((inb(0x2f8 + 5) & 0x20) == 0) { }
    outb(0x2f8, (uint8_t)value);
}
int serial_read_char(void) {
    return (inb(COM1 + 5) & 1) ? inb(COM1) : -1;
}
void trace_text(const char *text) {
    for (const char *p = text; *p; p++) { trace_char(*p); }
}
void trace_number(uint64_t value) {
    char buffer[21];
    unsigned i = 20;
    buffer[i] = 0;
    do { buffer[--i] = (char)('0' + value % 10); value /= 10; } while (value);
    trace_text(&buffer[i]);
}
void trace_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    trace_text("0x");
    for (int i = 60; i >= 0; i -= 4) { trace_char(digits[(value >> i) & 15]); }
}
#endif

__attribute__((noreturn))
void panic(const char *message) {
    __asm__ volatile ("cli" : : : "memory");
    serial_write("MiniLinux PANIC: ");
    serial_write(message);
    serial_write("\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

__attribute__((noreturn))
void kernel_main(void) {
    uart_init(COM1);
#if MINILINUX_LEVEL == 7
    uart_init(0x2f8);
#endif
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        panic("Limine base revision 6 is unavailable");
    }
#if MINILINUX_LEVEL >= 1
#if MINILINUX_LEVEL == 7
    serial_write("MiniLinux LAB: entered kernel_main\n");
#else
    serial_write("MiniLinux M"); serial_write_number(MINILINUX_LEVEL);
    serial_write(": entered kernel_main\n");
#endif
    if (memory_map_request.response == 0 || hhdm_request.response == 0) {
        panic("Limine memory map or HHDM response is unavailable");
    }
    if (!page_init(memory_map_request.response, hhdm_request.response->offset)) {
        panic("physical page initialization failed");
    }
#if MINILINUX_LEVEL == 1
    page_selftest(memory_map_request.response, hhdm_request.response->offset);
    panic("M1 reached its intentional stop");
#elif MINILINUX_LEVEL == 2
    vm_init(hhdm_request.response->offset);
    vm_selftest();
    panic("M2 reached its intentional stop");
#elif MINILINUX_LEVEL >= 3
    vm_init(hhdm_request.response->offset);
    cpu_init();
#if MINILINUX_ATTACK != 0
    counterexample_selftest();
#endif
    task_start();
#endif
#else
    serial_write("MiniLinux M0: entered kernel_main\n");
    panic("M0 reached its intentional stop");
#endif
}
