#ifndef MINILINUX_RAMFS_H
#define MINILINUX_RAMFS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { OPEN_LIMIT = 2 };
struct open_file { bool used; unsigned file; size_t offset; };
int64_t ramfs_open(struct open_file *slots, const char *name, size_t length);
int64_t ramfs_read(struct open_file *slots, uint64_t fd, char *out, size_t length);
int64_t ramfs_close(struct open_file *slots, uint64_t fd);
struct file_info;
int64_t ramfs_file(unsigned index, struct file_info *info);
#endif
