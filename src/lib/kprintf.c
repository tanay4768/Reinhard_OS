#include "kprintf.h"

#include "kstring.h"
#include "vga.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*out_fn)(char c, void *ctx);

static void emit_number(out_fn out, void *ctx, uint32_t value, unsigned base, bool upper,
                        bool negative, int width, char pad)
{
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *digits = upper ? upper_digits : lower_digits;

    char tmp[34];
    int  n = 0;
    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value);

    int total = n + (negative ? 1 : 0);
    if (negative && pad == '0')
        out('-', ctx);
    for (int i = total; i < width; i++)
        out(pad, ctx);
    if (negative && pad != '0')
        out('-', ctx);
    while (n--)
        out(tmp[n], ctx);
}

static void emit_string(out_fn out, void *ctx, const char *s, int width, bool left)
{
    int len = (int)strlen(s);
    if (!left)
        for (int i = len; i < width; i++)
            out(' ', ctx);
    for (; *s; s++)
        out(*s, ctx);
    if (left)
        for (int i = len; i < width; i++)
            out(' ', ctx);
}

static void vformat(out_fn out, void *ctx, const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out(*fmt, ctx);
            continue;
        }
        fmt++;

        bool left = false;
        char pad  = ' ';
        int  width = 0;

        if (*fmt == '-') { left = true; fmt++; }
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 'c':
            out((char)va_arg(ap, int), ctx);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            emit_string(out, ctx, s ? s : "(null)", width, left);
            break;
        }
        case 'd':
        case 'i': {
            int32_t  v   = va_arg(ap, int32_t);
            bool     neg = v < 0;
            uint32_t u   = neg ? (uint32_t)(-(v + 1)) + 1u : (uint32_t)v;
            emit_number(out, ctx, u, 10, false, neg, width, pad);
            break;
        }
        case 'u':
            emit_number(out, ctx, va_arg(ap, uint32_t), 10, false, false, width, pad);
            break;
        case 'x':
        case 'X':
            emit_number(out, ctx, va_arg(ap, uint32_t), 16, *fmt == 'X', false, width, pad);
            break;
        case 'p':
            out('0', ctx);
            out('x', ctx);
            emit_number(out, ctx, (uint32_t)(uintptr_t)va_arg(ap, void *), 16, false, false, 8, '0');
            break;
        case '%':
            out('%', ctx);
            break;
        case '\0':
            return;
        default:
            out('%', ctx);
            out(*fmt, ctx);
            break;
        }
    }
}

/* ---- sinks ---- */

static void vga_sink(char c, void *ctx)
{
    (void)ctx;
    vga_putc(c);
}

typedef struct {
    char  *buf;
    size_t cap;
    size_t pos;
} buf_sink_t;

static void buf_put(char c, void *ctx)
{
    buf_sink_t *s = ctx;
    if (s->pos + 1 < s->cap)
        s->buf[s->pos] = c;
    s->pos++;
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vformat(vga_sink, NULL, fmt, ap);
    va_end(ap);
}

int ksnprintf(char *buf, size_t cap, const char *fmt, ...)
{
    buf_sink_t sink = { buf, cap, 0 };
    va_list ap;
    va_start(ap, fmt);
    vformat(buf_put, &sink, fmt, ap);
    va_end(ap);

    if (cap > 0)
        buf[sink.pos < cap ? sink.pos : cap - 1] = '\0';
    return (int)sink.pos;
}
