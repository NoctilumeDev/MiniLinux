#include <stddef.h>
#include <stdbool.h>
#include "abi.h"

/* Only this wrapper knows the register convention of our own ABI. */
static int64_t call(uint64_t number, uint64_t a, uint64_t b, uint64_t c) {
    __asm__ volatile ("int $0x80" : "+a"(number) : "D"(a), "S"(b), "d"(c) : "memory", "cc");
    return (int64_t)number;
}
static uint64_t pointer(const void *p) { return (uint64_t)(uintptr_t)p; }
static size_t length(const char *s) { size_t n = 0; while (s[n]) { n++; } return n; }
static bool equal(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] && a[i] == b[i]) { i++; }
    return a[i] == b[i];
}
static void write_bytes(const char *text, size_t n) {
    while (n) {
        size_t part = n > COPY_LIMIT ? COPY_LIMIT : n;
        call(SYS_WRITE, pointer(text), part, 0); text += part; n -= part;
    }
}
static void print(const char *s) { write_bytes(s, length(s)); }
static void character(char c) { write_bytes(&c, 1); }

/* Small line builder keeps output from concurrent programs intact. */
struct line { char bytes[192]; size_t used; };
static void text(struct line *line, const char *s) {
    while (*s && line->used < sizeof(line->bytes)) { line->bytes[line->used++] = *s++; }
}
static void number(struct line *line, uint64_t n) {
    char digits[21]; unsigned i = 20; digits[i] = 0;
    do { digits[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    text(line, &digits[i]);
}
static void hex(struct line *line, uint64_t n) {
    static const char digits[] = "0123456789abcdef";
    text(line, "0x");
    for (int i = 60; i >= 0; i -= 4) {
        if (line->used < sizeof(line->bytes)) { line->bytes[line->used++] = digits[(n >> i) & 15]; }
    }
}
static void finish(struct line *line) { text(line, "\n"); write_bytes(line->bytes, line->used); }
static const char *program_name(uint64_t program) {
    switch (program) {
    case PROGRAM_INIT: return "init";
    case PROGRAM_SHELL: return "shell";
    case PROGRAM_HELLO: return "hello";
    case PROGRAM_READER: return "reader";
    case PROGRAM_COUNTER_A: return "counter-a";
    case PROGRAM_COUNTER_B: return "counter-b";
    case PROGRAM_FAULT: return "fault";
    default: return "unknown";
    }
}

static void cat(const char *name) {
    char path[96]; size_t n = length(name);
    if (n > 90 || !n) { print("cat: expected a short file name\n"); return; }
    size_t start = name[0] == '/' ? 0 : 1;
    if (start) { path[0] = '/'; }
    for (size_t i = 0; i < n; i++) { path[start + i] = name[i]; }
    int64_t fd = call(SYS_OPEN, pointer(path), start + n, 0);
    if (fd < 0) { print("cat: file not found\n"); return; }
    char bytes[128];
    for (;;) {
        int64_t read = call(SYS_READ, (uint64_t)fd, pointer(bytes), sizeof(bytes));
        if (read <= 0) { if (read < 0) { print("cat: read failed\n"); } break; }
        write_bytes(bytes, (size_t)read);
    }
    call(SYS_CLOSE, (uint64_t)fd, 0, 0);
}

static void list_files(void) {
    struct file_info info = {0};
    for (unsigned i = 0; call(SYS_FILE, i, pointer(&info), 0) == 1; i++) {
        struct line line = { .used = 0 };
        text(&line, info.name); text(&line, "  "); number(&line, info.size); text(&line, " bytes"); finish(&line);
    }
}

static void processes(void) {
    print("PID  PPID  STATE    PROGRAM     CR3\n");
    for (unsigned i = 0; i < 8; i++) {
        struct process_info info = {0};
        if (call(SYS_PROCESS, i, pointer(&info), 0) != 1) { continue; }
        struct line line = { .used = 0 };
        number(&line, info.pid); text(&line, "    "); number(&line, info.parent); text(&line, "     ");
        text(&line, info.state == PROCESS_WAITING ? "waiting  " : info.state == PROCESS_EXITED ? "exited   " : "ready    ");
        text(&line, program_name(info.program)); text(&line, "  "); hex(&line, info.root); finish(&line);
    }
}

static void memory(void) {
    struct memory_info info = {0};
    if (call(SYS_MEMORY, pointer(&info), 0, 0) != 0) { return; }
    struct line line = { .used = 0 };
    text(&line, "frames="); number(&line, info.total); text(&line, " free="); number(&line, info.free);
    finish(&line);
}

static int program_id(const char *name) {
    for (unsigned i = PROGRAM_HELLO; i < PROGRAM_COUNT; i++) {
        if (equal(name, program_name(i))) { return (int)i; }
    }
    return -1;
}

static void run(const char *first, const char *second) {
    int a = program_id(first), b = second ? program_id(second) : -1;
    if (a < 0 || (second && b < 0)) { print("run: unknown embedded program\n"); return; }
    int64_t pid_a = call(SYS_SPAWN, (unsigned)a, 0, 0);
    int64_t pid_b = second ? call(SYS_SPAWN, (unsigned)b, 0, 0) : 0;
    if (pid_a < 0 || pid_b < 0) { print("run: no free process slot or pages\n"); }
    if (pid_a > 0) {
        int64_t status = call(SYS_WAIT, (uint64_t)pid_a, 0, 0);
        struct line line = { .used = 0 };
        text(&line, "run: pid "); number(&line, (uint64_t)pid_a); text(&line, " exited status ");
        number(&line, (uint64_t)status); finish(&line);
    }
    if (pid_b > 0) {
        int64_t status = call(SYS_WAIT, (uint64_t)pid_b, 0, 0);
        struct line line = { .used = 0 };
        text(&line, "run: pid "); number(&line, (uint64_t)pid_b); text(&line, " exited status ");
        number(&line, (uint64_t)status); finish(&line);
    }
}

static void boundary_check(void) {
    /* A bad pointer must not consume input or file bytes. */
    bool okay = call(99, 0, 0, 0) == -1 &&
        call(SYS_INPUT, USER_CODE, 0, 0) == -2 &&
        call(SYS_WRITE, KERNEL_PROBE, 1, 0) == -2 &&
        call(SYS_SPAWN, UINT64_MAX, 0, 0) == -3 &&
        call(SYS_WAIT, UINT64_MAX, 0, 0) == -3 &&
        call(SYS_PROCESS, UINT64_MAX, USER_DATA + 64, 0) == 0;
    char buffer[32];
    const char *name = "/hello.txt";
    int64_t fd = call(SYS_OPEN, pointer(name), length(name), 0);
    okay = okay && fd >= 0 && call(SYS_READ, (uint64_t)fd, USER_CODE, 1) == -2;
    int64_t count = call(SYS_READ, (uint64_t)fd, pointer(buffer), sizeof(buffer));
    okay = okay && count == 17 && buffer[0] == 'h' && buffer[16] == '\n';
    call(SYS_CLOSE, (uint64_t)fd, 0, 0);
    print(okay ? "check: user boundaries passed\n" : "check: FAILED\n");
    struct memory_info before = {0}, after = {0};
    call(SYS_MEMORY, pointer(&before), 0, 0);
    int64_t children[6];
    for (unsigned i = 0; i < 6; i++) {
        children[i] = call(SYS_SPAWN, PROGRAM_HELLO, 0, 0);
        if (children[i] <= 0) { okay = false; }
    }
    if (call(SYS_SPAWN, PROGRAM_HELLO, 0, 0) != -5) { okay = false; }
    for (unsigned i = 0; i < 6; i++) {
        if (children[i] > 0 && call(SYS_WAIT, (uint64_t)children[i], 0, 0) != 0) { okay = false; }
    }
    call(SYS_MEMORY, pointer(&after), 0, 0);
    if (before.free != after.free) { okay = false; }
    print(okay ? "check: process capacity and reclamation passed\n" : "check: capacity FAILED\n");
}

/* Independent supplemental fixture; production kernel and ABI stay unchanged. */
static void supplemental_check(void) {
    volatile unsigned char *edge = (volatile unsigned char *)(uintptr_t)(USER_DATA + 4096 - 8);
    uint64_t cross = USER_STACK + 4096 - 8;
    uint64_t self = (uint64_t)call(SYS_TASK_ID, 0, 0, 0);
    struct memory_info before = {0}, after = {0};
    bool okay = true;
    int64_t rc;
    call(SYS_MEMORY, pointer(&before), 0, 0);
    for (unsigned i = 0; i < 8; i++) { edge[i] = (unsigned char)(0xa0 + i); }
    rc = call(SYS_PROCESS, 1, pointer((const void *)edge), 0);
    if (rc != -2) { okay = false; }
    for (unsigned i = 0; i < 8; i++) { if (edge[i] != 0xa0 + i) { okay = false; } }
    print(rc == -2 && okay ? "PROBE PROCESS cross-page rejected intact\n" : "PROBE PROCESS FAILED\n");
    for (unsigned i = 0; i < 8; i++) { edge[i] = (unsigned char)(0xb0 + i); }
    rc = call(SYS_FILE, 0, pointer((const void *)edge), 0);
    bool file_ok = rc == -2;
    for (unsigned i = 0; i < 8; i++) { if (edge[i] != 0xb0 + i) { file_ok = false; } }
    if (!file_ok) { okay = false; }
    print(file_ok ? "PROBE FILE cross-page rejected intact\n" : "PROBE FILE FAILED\n");
    for (unsigned i = 0; i < 8; i++) { edge[i] = (unsigned char)(0xc0 + i); }
    rc = call(SYS_MEMORY, pointer((const void *)edge), 0, 0);
    bool memory_ok = rc == -2;
    for (unsigned i = 0; i < 8; i++) { if (edge[i] != 0xc0 + i) { memory_ok = false; } }
    if (!memory_ok) { okay = false; }
    print(memory_ok ? "PROBE MEMORY cross-page rejected intact\n" : "PROBE MEMORY FAILED\n");

    bool valid = call(SYS_PROCESS, 1, cross, 0) == 1;
    valid = valid && ((struct process_info *)(uintptr_t)cross)->pid == self;
    rc = call(SYS_FILE, 0, cross, 0);
    valid = valid && rc == 1 && ((struct file_info *)(uintptr_t)cross)->size == 17;
    rc = call(SYS_MEMORY, cross, 0, 0);
    valid = valid && rc == 0 && ((struct memory_info *)(uintptr_t)cross)->free == before.free;
    if (!valid) { okay = false; }
    print(valid ? "PROBE mapped cross-page outputs passed\n" : "PROBE mapped outputs FAILED\n");

    int64_t non_child = call(SYS_WAIT, 1, 0, 0);
    int64_t own = call(SYS_WAIT, self, 0, 0);
    int64_t child = call(SYS_SPAWN, PROGRAM_HELLO, 0, 0);
    int64_t first_wait = child > 0 ? call(SYS_WAIT, (uint64_t)child, 0, 0) : -99;
    int64_t reaped_wait = child > 0 ? call(SYS_WAIT, (uint64_t)child, 0, 0) : -99;
    bool waits = non_child == -3 && own == -3 && child > 0 && first_wait == 0 && reaped_wait == -3;
    if (!waits) { okay = false; }
    print(waits ? "PROBE wait non-child self reaped rejected -3\n" : "PROBE wait FAILED\n");
    call(SYS_MEMORY, pointer(&after), 0, 0);
    if (before.free != after.free) { okay = false; }
    print(okay ? "SUPPLEMENTAL PROBE PASSED; pages restored\n" : "SUPPLEMENTAL PROBE FAILED\n");
}

static void supplemental_input(void) {
    print("PROBE INPUT READY: send Z\n");
    uint64_t until = (uint64_t)call(SYS_TICKS, 0, 0, 0) + 100;
    while ((uint64_t)call(SYS_TICKS, 0, 0, 0) < until) { }
    int64_t rejected = call(SYS_INPUT, USER_CODE, 0, 0);
    char c = 0;
    int64_t read;
    until = (uint64_t)call(SYS_TICKS, 0, 0, 0) + 200;
    do { read = call(SYS_INPUT, pointer(&c), 0, 0); }
    while (read == -6 && (uint64_t)call(SYS_TICKS, 0, 0, 0) < until);
    print(rejected == -2 && read == 1 && c == 'Z' ?
          "PROBE INPUT bad pointer preserved queued Z\n" : "PROBE INPUT FAILED\n");
}

static void command(char *line) {
    char *words[4]; unsigned count = 0; bool inside = false;
    for (size_t i = 0; line[i]; i++) {
        if (line[i] == ' ' || line[i] == '\t') { line[i] = 0; inside = false; }
        else if (!inside) {
            if (count == 4) { print("shell: too many arguments\n"); return; }
            words[count++] = &line[i]; inside = true;
        }
    }
    if (!count) { return; }
    if (equal(words[0], "help") && count == 1) {
        print("help | ls | cat FILE | ps | run PROGRAM [PROGRAM]\n");
        print("programs: hello reader counter-a counter-b fault\n");
        print("observe: mem | check\n");
    } else if (equal(words[0], "ls") && count == 1) { list_files(); }
    else if (equal(words[0], "cat") && count == 2) { cat(words[1]); }
    else if (equal(words[0], "ps") && count == 1) { processes(); }
    else if (equal(words[0], "run") && (count == 2 || count == 3)) { run(words[1], count == 3 ? words[2] : 0); }
    else if (equal(words[0], "mem") && count == 1) { memory(); }
    else if (equal(words[0], "probe") && count == 1) { supplemental_check(); }
    else if (equal(words[0], "probe-input") && count == 1) { supplemental_input(); }
    else if (equal(words[0], "check") && count == 1) { boundary_check(); }
    else { print("shell: unknown command or arguments; type help\n"); }
}

__attribute__((noreturn)) static void shell(void) {
    print("MiniLinux / x86_64 teaching kernel\n");
    print("Type 'help' for commands.\n\n");
    char line[96];
    for (;;) {
        print("mini$ "); size_t used = 0; bool overflow = false;
        uint64_t last = 0;
        for (;;) {
            char c = 0;
            if (call(SYS_INPUT, pointer(&c), 0, 0) != 1) {
                uint64_t now = (uint64_t)call(SYS_TICKS, 0, 0, 0);
                if (now - last >= 50) { call(SYS_INSPECT, 0, 0, 0); last = now; }
                continue;
            }
            if (c == '\r' || c == '\n') { character('\n'); break; }
            if (c == '\b' || c == 127) {
                if (used && !overflow) { used--; print("\b \b"); }
            } else if (c >= 32 && c <= 126) {
                if (used + 1 < sizeof(line) && !overflow) { line[used++] = c; character(c); }
                else { overflow = true; }
            }
        }
        line[used] = 0;
        if (overflow) { print("shell: line too long (95 characters maximum)\n"); }
        else { command(line); }
        call(SYS_INSPECT, 0, 0, 0);
    }
}

static void counter(unsigned program) {
    uint64_t pid = (uint64_t)call(SYS_TASK_ID, 0, 0, 0);
    volatile uint64_t *data = (volatile uint64_t *)(uintptr_t)USER_DATA;
    uint64_t marker = program == PROGRAM_COUNTER_A ? 0xa110 : 0xb220;
    data[0] = marker; data[1] = 0;
    uint64_t start = (uint64_t)call(SYS_TICKS, 0, 0, 0);
    for (unsigned step = 0; step < 5; step++) {
        while ((uint64_t)call(SYS_TICKS, 0, 0, 0) - start < (step + 1) * 100) {
            for (unsigned i = 0; i < 10000; i++) { data[1]++; }
            if (data[0] != marker) { print("counter: ISOLATION FAILED\n"); call(SYS_EXIT, 2, 0, 0); }
        }
        struct line line = { .used = 0 };
        text(&line, program_name(program)); text(&line, " pid="); number(&line, pid);
        text(&line, " step="); number(&line, step + 1); text(&line, " marker="); hex(&line, data[0]);
        text(&line, " work="); number(&line, data[1]); finish(&line);
        call(SYS_INSPECT, 0, 0, 0);
    }
}

__attribute__((section(".text.entry"), noreturn)) void user_main(void) {
    unsigned program = (unsigned)call(SYS_PROGRAM, 0, 0, 0);
    switch (program) {
    case PROGRAM_INIT: {
        print("init: PID 1 entered user mode; starting shell\n");
        int64_t pid = call(SYS_SPAWN, PROGRAM_SHELL, 0, 0);
        if (pid > 0) { call(SYS_WAIT, (uint64_t)pid, 0, 0); }
        print("init: shell stopped\n"); break;
    }
    case PROGRAM_SHELL: shell();
    case PROGRAM_HELLO: print("hello from user mode\n"); break;
    case PROGRAM_READER: print("reader: reading /hello.txt through syscalls\n"); cat("/hello.txt"); break;
    case PROGRAM_COUNTER_A:
    case PROGRAM_COUNTER_B: counter(program); break;
    case PROGRAM_FAULT: print("fault: requesting a forbidden kernel read\n");
        { volatile uint8_t denied = *(volatile uint8_t *)(uintptr_t)KERNEL_PROBE; (void)denied; } break;
    }
    call(SYS_EXIT, 0, 0, 0);
    for (;;) { }
}
