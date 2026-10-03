#include "vga.h"

#include "io.h"

#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5

static size_t  cursor_row, cursor_col;
static uint8_t text_color;

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

static void clear_row(size_t y)
{
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[y * VGA_WIDTH + x] = cell(' ', text_color);
}

static void scroll_up(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    clear_row(VGA_HEIGHT - 1);
    cursor_row = VGA_HEIGHT - 1;
}

void vga_init(void)
{
    text_color = vga_color(VGA_LIGHT_GREY, VGA_BLACK);
    hw_cursor_enable();
    vga_clear();
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        clear_row(y);
    cursor_row = cursor_col = 0;
    hw_cursor_move(0, 0);
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
