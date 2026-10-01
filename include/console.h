#ifndef MINILINUX_CONSOLE_H
#define MINILINUX_CONSOLE_H

#include <stdint.h>

void serial_write(const char *message);
void serial_write_hex(uint64_t value);
void serial_write_number(uint64_t value);
__attribute__((noreturn)) void panic(const char *message);

#endif
