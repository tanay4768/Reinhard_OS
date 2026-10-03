#ifndef REINHARD_KSTRING_H
#define REINHARD_KSTRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void  *memset(void *dst, int value, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);

/* Copies at most cap-1 bytes and always NUL-terminates (unlike strncpy). */
void   strlcpy(char *dst, const char *src, size_t cap);

/* Parses decimal, or hex when prefixed with 0x. Returns false on bad input. */
bool   parse_uint(const char *s, uint32_t *out);

#endif
