#ifndef MINILINUX_CONSOLE_H
#define MINILINUX_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

void serial_write(const char *message);
void serial_write_bytes(const char *bytes, size_t length);
void serial_write_hex(uint64_t value);
void serial_write_number(uint64_t value);
#if MINILINUX_LEVEL == 7
int serial_read_char(void);
void trace_text(const char *text);
void trace_number(uint64_t value);
void trace_hex(uint64_t value);
#endif
__attribute__((noreturn)) void panic(const char *message);

#endif
