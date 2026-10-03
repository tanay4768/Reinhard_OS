#ifndef REINHARD_KPRINTF_H
#define REINHARD_KPRINTF_H

#include <stddef.h>

/* Supported: %c %s %d %i %u %x %X %p %%  with optional '-', '0' and width. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int  ksnprintf(char *buf, size_t cap, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#endif
