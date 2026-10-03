/* Interactive shell: prompt, line editing, history, tokenising, dispatch. */

#include "shell.h"

#include "fs.h"
#include "keyboard.h"
#include "kprintf.h"
#include "kstring.h"
#include "vga.h"

#define LINE_MAX     128
#define ARGV_MAX     16
#define HISTORY_MAX  16

static char history[HISTORY_MAX][LINE_MAX];
static int  history_count;

static void history_add(const char *line)
{
    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0)
        return;                                       /* skip consecutive duplicates */

    if (history_count == HISTORY_MAX) {
        memmove(history[0], history[1], sizeof(history[0]) * (HISTORY_MAX - 1));
        history_count--;
    }
    strlcpy(history[history_count++], line, LINE_MAX);
}

static void print_prompt(void)
{
    char path[64];
    fs_path(fs_cwd(), path, sizeof(path));

    vga_set_color(vga_color(VGA_LIGHT_GREEN, VGA_BLACK));
    kprintf("akira");
    vga_set_color(vga_color(VGA_WHITE, VGA_BLACK));
    kprintf(":");
    vga_set_color(vga_color(VGA_LIGHT_CYAN, VGA_BLACK));
    kprintf("%s", path);
    vga_set_color(vga_color(VGA_WHITE, VGA_BLACK));
    kprintf("$ ");
    vga_set_color(vga_color(VGA_LIGHT_GREY, VGA_BLACK));
}

/* Replace what is currently typed on screen with `text`. */
static void replace_line(char *buf, size_t *len, const char *text)
{
    while (*len) {
        vga_putc('\b');
        (*len)--;
    }
    strlcpy(buf, text, LINE_MAX);
    *len = strlen(buf);
    vga_write(buf);
}

static void read_line(char *buf)
{
    size_t len = 0;
    int    hist_pos = history_count;                  /* one past the newest entry */
    buf[0] = '\0';

    for (;;) {
        int key = keyboard_getkey();

        if (key == '\n') {
            vga_putc('\n');
            buf[len] = '\0';
            return;
        } else if (key == '\b') {
            if (len > 0) {
                len--;
                vga_putc('\b');
            }
        } else if (key == KEY_UP) {
            if (hist_pos > 0)
                replace_line(buf, &len, history[--hist_pos]);
        } else if (key == KEY_DOWN) {
            if (hist_pos < history_count) {
                hist_pos++;
                replace_line(buf, &len, hist_pos == history_count ? "" : history[hist_pos]);
            }
        } else if (key == KEY_CTRL('l')) {
            vga_clear();
            print_prompt();
            buf[len] = '\0';
            vga_write(buf);
        } else if (key >= 32 && key < 127 && len < LINE_MAX - 1) {
            buf[len++] = (char)key;
            vga_putc((char)key);
        }
    }
}

/* Splits `line` in place on whitespace; "double quoted" text is one argument. */
static int tokenize(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;

        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"')
                p++;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
        }
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

static void execute(char *line)
{
    char *argv[ARGV_MAX];
    int   argc = tokenize(line, argv, ARGV_MAX);
    if (argc == 0)
        return;

    for (size_t i = 0; i < shell_command_count; i++) {
        if (strcmp(argv[0], shell_commands[i].name) == 0) {
            shell_commands[i].run(argc, argv);
            return;
        }
    }
    kprintf("akira: command not found: %s (try 'help')\n", argv[0]);
}

void shell_run(void)
{
    char line[LINE_MAX];

    for (;;) {
        print_prompt();
        read_line(line);
        if (line[0])
            history_add(line);
        execute(line);
    }
}
