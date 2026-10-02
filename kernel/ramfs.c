#include "bytes.h"
#include "ramfs.h"

struct file { const char *name, *bytes; size_t name_length, size; };
static const char hello[] = "hello from ramfs\n";
static const struct file files[] = {
    { "/hello.txt", hello, 10, sizeof(hello) - 1 },
    { "/empty.txt", "", 10, 0 }
};

int64_t ramfs_open(struct open_file *slots, const char *name, size_t length) {
    for (unsigned file = 0; file < sizeof(files) / sizeof(files[0]); file++) {
        if (length != files[file].name_length) { continue; }
        bool equal = true;
        for (size_t i = 0; i < length; i++) {
            if (name[i] != files[file].name[i]) { equal = false; break; }
        }
        if (!equal) { continue; }
        for (unsigned fd = 0; fd < OPEN_LIMIT; fd++) {
            if (!slots[fd].used) {
                slots[fd].used = true;
                slots[fd].file = file;
                slots[fd].offset = 0;
                return fd;
            }
        }
        return -5; /* This task has no free open slot. */
    }
    return -4; /* No such file. */
}

int64_t ramfs_read(struct open_file *slots, uint64_t fd, char *out, size_t length) {
    if (fd >= OPEN_LIMIT || !slots[fd].used) { return -3; }
    struct open_file *open = &slots[fd];
    const struct file *file = &files[open->file];
    size_t available = file->size - open->offset;
    if (length > available) { length = available; }
    memcpy(out, file->bytes + open->offset, length);
    open->offset += length;
    return (int64_t)length;
}

int64_t ramfs_close(struct open_file *slots, uint64_t fd) {
    if (fd >= OPEN_LIMIT || !slots[fd].used) { return -3; }
    slots[fd].used = false;
    return 0;
}
