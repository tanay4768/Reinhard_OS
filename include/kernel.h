#ifndef REINHARD_KERNEL_H
#define REINHARD_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REINHARD_VERSION "0.2.0"

#define PAGE_SIZE 4096u

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define UNUSED(x)        ((void)(x))

struct registers;

/* Print a diagnostic (and register dump when available) and halt the CPU. */
void kernel_panic(const char *msg, const struct registers *regs) __attribute__((noreturn));
#define PANIC(msg) kernel_panic((msg), NULL)

#endif
