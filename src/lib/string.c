#include "kstring.h"

/* NOTE: the build passes -fno-tree-loop-distribute-patterns so GCC does not
 * turn these loops back into calls to memset/memcpy (infinite recursion). */

void *memset(void *dst, int value, size_t n)
{
    uint8_t *d = dst;
    while (n--)
        *d++ = (uint8_t)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t       *d = dst;
    const uint8_t *s = src;
    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a, *y = b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i])
            return x[i] - y[i];
    return 0;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n ? (uint8_t)*a - (uint8_t)*b : 0;
}

char *strchr(const char *s, int c)
{
    for (; *s; s++)
        if (*s == (char)c)
            return (char *)s;
    return c == 0 ? (char *)s : NULL;
}

void strlcpy(char *dst, const char *src, size_t cap)
{
    if (cap == 0)
        return;
    size_t i = 0;
    for (; i + 1 < cap && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

bool parse_uint(const char *s, uint32_t *out)
{
    uint32_t base = 10, value = 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (*s == '\0')
        return false;

    for (; *s; s++) {
        uint32_t digit;
        if (*s >= '0' && *s <= '9')       digit = (uint32_t)(*s - '0');
        else if (*s >= 'a' && *s <= 'f')  digit = (uint32_t)(*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F')  digit = (uint32_t)(*s - 'A' + 10);
        else return false;

        if (digit >= base)
            return false;
        value = value * base + digit;
    }
    *out = value;
    return true;
}
