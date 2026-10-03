#ifndef AKIRA_KEYBOARD_H
#define AKIRA_KEYBOARD_H

/* Printable keys are returned as ASCII; Ctrl+<letter> as 1..26. */
enum {
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_DELETE, KEY_PGUP, KEY_PGDN
};

#define KEY_CTRL(c) ((c) - 'a' + 1)

void keyboard_init(void);
int  keyboard_getkey(void);   /* blocks (halting the CPU) until a key arrives */

#endif
