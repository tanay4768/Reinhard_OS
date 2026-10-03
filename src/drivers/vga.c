#include "vga.h"

#include "io.h"
#include "kstring.h"

#include <stdbool.h>

#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5

#define ATTR_REVERSE     0x70u          /* attribute bits that swap fg and bg */
#define SCROLLBACK_LINES 200u           /* lines kept above the screen (~32 KiB) */

static size_t  cursor_row, cursor_col;
static uint8_t text_color;

/* Scrollback state.  `history` is a ring of the lines that have scrolled off
 * the top; `live_screen` is a snapshot of the real console, taken before the
 * first page back and restored when returning, so paging never loses output. */
static uint16_t history[SCROLLBACK_LINES][VGA_WIDTH];
static unsigned history_count;         /* lines stored, at most SCROLLBACK_LINES */
static unsigned history_head;          /* ring slot holding the next line to store */
static unsigned scroll_offset;         /* lines between the viewport and the live screen */
static uint16_t live_screen[VGA_HEIGHT][VGA_WIDTH];
static bool     live_saved;

static inline uint16_t cell(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c | (uint16_t)((uint16_t)color << 8);
}

static void hw_cursor_move(size_t x, size_t y)
{
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);
    outb(CRTC_INDEX, 0x0F);
    outb(CRTC_DATA, (uint8_t)(pos & 0xFF));
    outb(CRTC_INDEX, 0x0E);
    outb(CRTC_DATA, (uint8_t)(pos >> 8));
}

static void hw_cursor_enable(void)
{
    outb(CRTC_INDEX, 0x0A);
    outb(CRTC_DATA, (uint8_t)((inb(CRTC_DATA) & 0xC0) | 14));   /* scanline start */
    outb(CRTC_INDEX, 0x0B);
    outb(CRTC_DATA, (uint8_t)((inb(CRTC_DATA) & 0xE0) | 15));   /* scanline end   */
}

/* Bit 5 of CRTC register 0x0A hides the hardware cursor without moving it. */
static void hw_cursor_visible(bool visible)
{
    outb(CRTC_INDEX, 0x0A);
    uint8_t v = inb(CRTC_DATA);
    outb(CRTC_DATA, visible ? (uint8_t)(v & ~0x20u) : (uint8_t)(v | 0x20u));
}

static void clear_row(size_t y)
{
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[y * VGA_WIDTH + x] = cell(' ', text_color);
}

/* ---- scrollback ---- */

/* The text buffer is volatile, so rows are moved cell by cell rather than with
 * memcpy, which would have to drop the qualifier. */
static void row_load(size_t y, uint16_t *dst)
{
    for (size_t x = 0; x < VGA_WIDTH; x++)
        dst[x] = VGA_MEMORY[y * VGA_WIDTH + x];
}

static void row_store(size_t y, const uint16_t *src)
{
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[y * VGA_WIDTH + x] = src[x];
}

/* Stores the top row, which is about to scroll off, in the ring. */
static void history_push_top(void)
{
    row_load(0, history[history_head]);
    history_head = (history_head + 1) % SCROLLBACK_LINES;
    if (history_count < SCROLLBACK_LINES)
        history_count++;
}

/* index 0 is the oldest line still kept. */
static const uint16_t *history_line(unsigned index)
{
    unsigned first = (history_head + SCROLLBACK_LINES - history_count) % SCROLLBACK_LINES;
    return history[(first + index) % SCROLLBACK_LINES];
}

static void save_live_screen(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        row_load(y, live_screen[y]);
    live_saved = true;
}

static void restore_live_screen(void)
{
    if (!live_saved)
        return;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        row_store(y, live_screen[y]);
    live_saved = false;
}

/* Repaints the viewport: `scroll_offset` lines of history, then as much of the
 * saved live screen as still fits on the screen. */
static void render_view(void)
{
    unsigned first = history_count - scroll_offset;     /* oldest visible line */

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        unsigned index = first + (unsigned)y;
        row_store(y, index < history_count ? history_line(index)
                                           : live_screen[y - scroll_offset]);
    }

    /* Invert the bottom row while there is more output below it. */
    if (scroll_offset) {
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] ^= (uint16_t)(ATTR_REVERSE << 8);
    }
}

void vga_scroll(int lines)
{
    int offset = (int)scroll_offset + lines;

    if (offset < 0)
        offset = 0;
    if (offset > (int)history_count)
        offset = (int)history_count;

    if (offset == 0) {
        if (!scroll_offset)
            return;                       /* already live: leave the cursor alone */
        scroll_offset = 0;
        restore_live_screen();
        hw_cursor_move(cursor_col, cursor_row);
        hw_cursor_visible(true);
        return;
    }

    if (!live_saved)
        save_live_screen();
    scroll_offset = (unsigned)offset;
    render_view();
    hw_cursor_visible(false);
}

void vga_scroll_reset(void)
{
    vga_scroll(-(int)scroll_offset);
}

bool vga_scrolled(void)
{
    return scroll_offset != 0;
}

static void scroll_up(void)
{
    /* Output means the user is watching the live screen again. */
    vga_scroll_reset();

    history_push_top();
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    clear_row(VGA_HEIGHT - 1);
    cursor_row = VGA_HEIGHT - 1;
}

void vga_init(void)
{
    text_color = vga_color(VGA_LIGHT_GREY, VGA_BLACK);
    history_count = 0;
    history_head = 0;
    hw_cursor_enable();
    vga_clear();
}

void vga_clear(void)
{
    scroll_offset = 0;
    restore_live_screen();
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        clear_row(y);
    cursor_row = cursor_col = 0;
    hw_cursor_move(0, 0);
    hw_cursor_visible(true);
}

void vga_set_color(uint8_t color) { text_color = color; }
uint8_t vga_get_color(void)       { return text_color; }

void vga_put_at(size_t x, size_t y, char c, uint8_t color)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT)
        VGA_MEMORY[y * VGA_WIDTH + x] = cell(c, color);
}

void vga_set_cursor(size_t x, size_t y)
{
    hw_cursor_move(x, y);
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        cursor_col = 0;
        cursor_row++;
        break;
    case '\r':
        cursor_col = 0;
        break;
    case '\b':
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        } else {
            break;
        }
        vga_put_at(cursor_col, cursor_row, ' ', text_color);
        break;
    case '\t': {
        size_t spaces = 4 - (cursor_col % 4);
        while (spaces--)
            vga_putc(' ');
        return;
    }
    default:
        vga_put_at(cursor_col, cursor_row, c, text_color);
        cursor_col++;
        break;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= VGA_HEIGHT)
        scroll_up();

    hw_cursor_move(cursor_col, cursor_row);
}

void vga_write(const char *s)
{
    while (*s)
        vga_putc(*s++);
}
