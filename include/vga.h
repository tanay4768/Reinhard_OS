#ifndef REINHARD_VGA_H
#define REINHARD_VGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

typedef enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN, VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
} vga_color_t;

static inline uint8_t vga_color(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

void    vga_init(void);
void    vga_clear(void);
void    vga_putc(char c);
void    vga_write(const char *s);
void    vga_set_color(uint8_t color);
uint8_t vga_get_color(void);

/* Scrollback: every line that scrolls off the top is kept, so the shell can
 * page back through earlier output.  vga_scroll() takes a signed line count
 * (positive looks further back) and clamps to what is still stored. */
void    vga_scroll(int lines);
void    vga_scroll_reset(void);   /* return to the live screen */
bool    vga_scrolled(void);       /* true while showing history, not the live screen */

/* Direct cell access, used by full-screen programs such as the notepad. */
void    vga_put_at(size_t x, size_t y, char c, uint8_t color);
void    vga_set_cursor(size_t x, size_t y);

#endif
