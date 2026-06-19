#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {

// Linux RISC-V syscall numbers
static long syscall0(long n) {
    register long a7 asm("a7") = n;
    register long a0 asm("a0");
    asm volatile ("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static long syscall1(long n, long a) {
    register long a7 asm("a7") = n;
    register long a0 asm("a0") = a;
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static long syscall3(long n, long a, long b, long c) {
    register long a7 asm("a7") = n;
    register long a0 asm("a0") = a;
    register long a1 asm("a1") = b;
    register long a2 asm("a2") = c;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

static long syscall4(long n, long a, long b, long c, long d) {
    register long a7 asm("a7") = n;
    register long a0 asm("a0") = a;
    register long a1 asm("a1") = b;
    register long a2 asm("a2") = c;
    register long a3 asm("a3") = d;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
    return a0;
}

static int convert_result(long ret) {
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (int)ret;
}

// ------------------------------------------------------------
// File I/O syscalls for newlib / unknown-elf
// ------------------------------------------------------------

int _open(const char *path, int flags, int mode) {
    // Linux openat syscall = 56
    // AT_FDCWD = -100
    long ret = syscall4(56, -100, (long)path, flags, mode);
    return convert_result(ret);
}

int _close(int fd) {
    // close syscall = 57
    long ret = syscall1(57, fd);
    return convert_result(ret);
}

_ssize_t _read(int fd, void *buf, size_t count) {
    // read syscall = 63
    long ret = syscall3(63, fd, (long)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (_ssize_t)ret;
}

_ssize_t _write(int fd, const void *buf, size_t count) {
    // write syscall = 64
    long ret = syscall3(64, fd, (long)buf, count);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (_ssize_t)ret;
}

_off_t _lseek(int fd, _off_t offset, int whence) {
    // lseek syscall = 62
    long ret = syscall3(62, fd, offset, whence);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return (_off_t)ret;
}

int _fstat(int fd, struct stat *st) {
    // Minimal implementation.
    // This is enough for many newlib stdio operations.
    (void)fd;
    if (st) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}

int _stat(const char *path, struct stat *st) {
    (void)path;
    if (st) {
        st->st_mode = S_IFREG;
    }
    return 0;
}

int _isatty(int fd) {
    return (fd == 0 || fd == 1 || fd == 2);
}

// ------------------------------------------------------------
// Heap support for malloc / aligned_alloc / vector allocations
// ------------------------------------------------------------

void *_sbrk(ptrdiff_t incr) {
    static unsigned char heap[128 * 1024 * 1024]; // 128 MB heap
    static size_t used = 0;

    if (incr < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }

    if (used + (size_t)incr > sizeof(heap)) {
        errno = ENOMEM;
        return (void *)-1;
    }

    void *prev = &heap[used];
    used += (size_t)incr;
    return prev;
}

// ------------------------------------------------------------
// Other required stubs
// ------------------------------------------------------------

int _getpid(void) {
    return 1;
}

int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

void _exit(int status) {
    // exit syscall = 93
    syscall1(93, status);
    while (1) {}
}

}

