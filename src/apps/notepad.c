/* Notepad: a small full-screen text editor.
 *
 * The document is one flat character buffer; "lines" are just runs between
 * '\n'. All drawing goes straight to VGA cells. Layout:
 *   row 0        title bar
 *   rows 1..23   text area (scrolls vertically and horizontally)
 *   row 24       status / help bar
 */

#include "notepad.h"

#include "fs.h"
#include "heap.h"
#include "keyboard.h"
#include "kernel.h"
#include "kprintf.h"
#include "kstring.h"
#include "vga.h"

#define TEXT_TOP   1
#define TEXT_ROWS  (VGA_HEIGHT - 2)
#define STATUS_ROW (VGA_HEIGHT - 1)
#define TAB_WIDTH  4
#define INITIAL_SLACK 1024

typedef struct {
    char   *buf;
    size_t  len, cap;
    size_t  cursor;            /* index into buf                              */
    size_t  top;               /* index of the first character on screen      */
    size_t  left;              /* horizontal scroll offset (columns)          */
    size_t  want_col;          /* column to return to on Up/Down              */
    bool    modified;
    bool    quit_armed;
    char    path[96];
    char    message[48];
} editor_t;

/* ---------- buffer helpers ---------- */

static size_t line_start(const editor_t *ed, size_t i)
{
    while (i > 0 && ed->buf[i - 1] != '\n')
        i--;
    return i;
}

static size_t line_end(const editor_t *ed, size_t i)
{
    while (i < ed->len && ed->buf[i] != '\n')
        i++;
    return i;
}

static void set_message(editor_t *ed, const char *msg)
{
    strlcpy(ed->message, msg, sizeof(ed->message));
}

static bool ensure_capacity(editor_t *ed, size_t extra)
{
    if (ed->len + extra + 1 <= ed->cap)
        return true;

    size_t new_cap = ed->cap * 2;
    if (new_cap < ed->len + extra + 1)
        new_cap = ed->len + extra + 1;

    char *grown = krealloc(ed->buf, new_cap);
    if (!grown)
        return false;
    ed->buf = grown;
    ed->cap = new_cap;
    return true;
}

static void sync_want_col(editor_t *ed)
{
    ed->want_col = ed->cursor - line_start(ed, ed->cursor);
}

static void insert_char(editor_t *ed, char c)
{
    if (ed->len >= FS_MAX_FILE_SIZE) {
        set_message(ed, "File size limit reached");
        return;
    }
    if (!ensure_capacity(ed, 1)) {
        set_message(ed, "Out of memory");
        return;
    }
    memmove(ed->buf + ed->cursor + 1, ed->buf + ed->cursor, ed->len - ed->cursor);
    ed->buf[ed->cursor++] = c;
    ed->len++;
    ed->modified = true;
    sync_want_col(ed);
}

static void delete_at(editor_t *ed, size_t index)
{
    memmove(ed->buf + index, ed->buf + index + 1, ed->len - index - 1);
    ed->len--;
    ed->modified = true;
}

/* ---------- cursor movement ---------- */

static void move_up(editor_t *ed)
{
    size_t start = line_start(ed, ed->cursor);
    if (start == 0)
        return;
    size_t prev_end   = start - 1;
    size_t prev_start = line_start(ed, prev_end);
    size_t prev_len   = prev_end - prev_start;
    ed->cursor = prev_start + (ed->want_col < prev_len ? ed->want_col : prev_len);
}

static void move_down(editor_t *ed)
{
    size_t end = line_end(ed, ed->cursor);
    if (end >= ed->len)
        return;
    size_t next_start = end + 1;
    size_t next_len   = line_end(ed, next_start) - next_start;
    ed->cursor = next_start + (ed->want_col < next_len ? ed->want_col : next_len);
}

/* ---------- rendering ---------- */

static void draw_bar(size_t row, const char *left_text, const char *right_text, uint8_t color)
{
    size_t right_len = strlen(right_text);
    size_t left_len  = strlen(left_text);

    for (size_t x = 0; x < VGA_WIDTH; x++) {
        char c = ' ';
        if (x < left_len)
            c = left_text[x];
        else if (x >= VGA_WIDTH - right_len)
            c = right_text[x - (VGA_WIDTH - right_len)];
        vga_put_at(x, row, c, color);
    }
}

static void scroll_to_cursor(editor_t *ed)
{
    /* Vertical: `top` always sits at the start of a line. */
    if (ed->cursor < ed->top) {
        ed->top = line_start(ed, ed->cursor);
    } else {
        size_t row = 0;
        for (size_t i = ed->top; i < ed->cursor; i++)
            if (ed->buf[i] == '\n')
                row++;
        while (row >= TEXT_ROWS) {
            while (ed->buf[ed->top] != '\n')
                ed->top++;
            ed->top++;
            row--;
        }
    }

    /* Horizontal. */
    size_t col = ed->cursor - line_start(ed, ed->cursor);
    if (col < ed->left)
        ed->left = col;
    else if (col >= ed->left + VGA_WIDTH)
        ed->left = col - VGA_WIDTH + 1;
}

static void render(editor_t *ed)
{
    const uint8_t text_color = vga_color(VGA_LIGHT_GREY, VGA_BLACK);
    const uint8_t bar_color  = vga_color(VGA_BLACK, VGA_LIGHT_GREY);

    scroll_to_cursor(ed);

    char title[VGA_WIDTH + 1];
    ksnprintf(title, sizeof(title), " Reinhard  Notepad - %s%s", ed->path, ed->modified ? " [modified]" : "");
    draw_bar(0, title, "", vga_color(VGA_WHITE, VGA_BLUE));

    size_t idx = ed->top;
    for (size_t r = 0; r < TEXT_ROWS; r++) {
        if (idx > ed->len) {
            for (size_t x = 0; x < VGA_WIDTH; x++)
                vga_put_at(x, TEXT_TOP + r, ' ', text_color);
            continue;
        }
        size_t end = line_end(ed, idx);
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t pos = idx + ed->left + x;
            char c = ' ';
            if (pos < end)
                c = (ed->buf[pos] >= 32 && ed->buf[pos] < 127) ? ed->buf[pos] : '.';
            vga_put_at(x, TEXT_TOP + r, c, text_color);
        }
        idx = end + 1;
    }

    size_t cur_line = 1;
    for (size_t i = 0; i < ed->cursor; i++)
        if (ed->buf[i] == '\n')
            cur_line++;
    size_t cur_col = ed->cursor - line_start(ed, ed->cursor);

    char left_text[VGA_WIDTH + 1], right_text[24];
    if (ed->message[0])
        ksnprintf(left_text, sizeof(left_text), " %s", ed->message);
    else
        ksnprintf(left_text, sizeof(left_text), " ^S Save  ^Q Quit");
    ksnprintf(right_text, sizeof(right_text), "Ln %u, Col %u ", (unsigned)cur_line, (unsigned)(cur_col + 1));
    draw_bar(STATUS_ROW, left_text, right_text, bar_color);

    size_t screen_row = 0;
    for (size_t i = ed->top; i < ed->cursor; i++)
        if (ed->buf[i] == '\n')
            screen_row++;
    vga_set_cursor(cur_col - ed->left, TEXT_TOP + screen_row);
}

/* ---------- file I/O ---------- */

static bool load_file(editor_t *ed, const char *path)
{
    fs_node_t *node = fs_lookup(path);

    if (node && node->type == FS_DIR) {
        kprintf("notepad: %s: Is a directory\n", path);
        return false;
    }

    size_t size = node ? node->size : 0;
    ed->cap = size + INITIAL_SLACK;
    ed->buf = kmalloc(ed->cap);
    if (!ed->buf) {
        kprintf("notepad: out of memory\n");
        return false;
    }
    ed->len = node ? fs_read(node, 0, ed->buf, size) : 0;
    strlcpy(ed->path, path, sizeof(ed->path));
    set_message(ed, node ? "" : "New file");
    return true;
}

static void save_file(editor_t *ed)
{
    fs_node_t *node = fs_lookup(ed->path);
    fs_err_t   err  = FS_OK;

    if (!node)
        node = fs_create(ed->path, FS_FILE, &err);
    if (node)
        err = fs_write(node, ed->buf, ed->len);

    if (err == FS_OK) {
        ed->modified = false;
        char msg[48];
        ksnprintf(msg, sizeof(msg), "Saved %u bytes", (unsigned)ed->len);
        set_message(ed, msg);
    } else {
        char msg[48];
        ksnprintf(msg, sizeof(msg), "Save failed: %s", fs_strerror(err));
        set_message(ed, msg);
    }
}

/* ---------- main loop ---------- */

int notepad_run(const char *path)
{
    editor_t *ed = kzalloc(sizeof(*ed));
    if (!ed)
        return 1;
    if (!load_file(ed, path)) {
        kfree(ed);
        return 1;
    }

    bool running = true;
    while (running) {
        render(ed);
        int key = keyboard_getkey();

        bool was_armed = ed->quit_armed;
        ed->quit_armed = false;
        if (key != KEY_CTRL('q'))
            ed->message[0] = '\0';

        switch (key) {
        case KEY_CTRL('s'):
            save_file(ed);
            break;
        case KEY_CTRL('q'):
            if (ed->modified && !was_armed) {
                set_message(ed, "Unsaved changes! Press ^Q again to discard");
                ed->quit_armed = true;
            } else {
                running = false;
            }
            break;
        case KEY_LEFT:
            if (ed->cursor > 0) ed->cursor--;
            sync_want_col(ed);
            break;
        case KEY_RIGHT:
            if (ed->cursor < ed->len) ed->cursor++;
            sync_want_col(ed);
            break;
        case KEY_UP:   move_up(ed);   break;
        case KEY_DOWN: move_down(ed); break;
        case KEY_PGUP:
            for (size_t i = 0; i < TEXT_ROWS; i++) move_up(ed);
            break;
        case KEY_PGDN:
            for (size_t i = 0; i < TEXT_ROWS; i++) move_down(ed);
            break;
        case KEY_HOME:
            ed->cursor = line_start(ed, ed->cursor);
            sync_want_col(ed);
            break;
        case KEY_END:
            ed->cursor = line_end(ed, ed->cursor);
            sync_want_col(ed);
            break;
        case KEY_DELETE:
            if (ed->cursor < ed->len) delete_at(ed, ed->cursor);
            break;
        case '\b':
            if (ed->cursor > 0) {
                delete_at(ed, ed->cursor - 1);
                ed->cursor--;
                sync_want_col(ed);
            }
            break;
        case '\n':
            insert_char(ed, '\n');
            break;
        case '\t':
            for (int i = 0; i < TAB_WIDTH; i++)
                insert_char(ed, ' ');
            break;
        default:
            if (key >= 32 && key < 127)
                insert_char(ed, (char)key);
            break;
        }
    }

    kfree(ed->buf);
    kfree(ed);
    vga_set_color(vga_color(VGA_LIGHT_GREY, VGA_BLACK));
    vga_clear();
    return 0;
}
