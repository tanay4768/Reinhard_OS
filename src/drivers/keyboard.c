#include "keyboard.h"

#include "io.h"
#include "isr.h"
#include "pic.h"

#include <stdbool.h>
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KEYMAP_SIZE   0x59
#define BUFFER_SIZE   256

/* Scancode set 1 -> ASCII, US layout. 0 means "no printable character". */
static const char keymap[KEYMAP_SIZE] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0, '*', 0, ' ', 0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const char keymap_shift[KEYMAP_SIZE] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
    'B','N','M','<','>','?', 0, '*', 0, ' ', 0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static volatile int      ring[BUFFER_SIZE];
static volatile unsigned head, tail;

static bool shift_down, ctrl_down, caps_lock, extended;

static void push_key(int key)
{
    unsigned next = (head + 1) % BUFFER_SIZE;
    if (next == tail)
        return;                       /* buffer full: drop the key */
    ring[head] = key;
    head = next;
}

static int translate_extended(uint8_t sc)
{
    switch (sc) {
    case 0x48: return KEY_UP;
    case 0x50: return KEY_DOWN;
    case 0x4B: return KEY_LEFT;
    case 0x4D: return KEY_RIGHT;
    case 0x47: return KEY_HOME;
    case 0x4F: return KEY_END;
    case 0x53: return KEY_DELETE;
    case 0x49: return KEY_PGUP;
    case 0x51: return KEY_PGDN;
    case 0x1C: return '\n';           /* keypad Enter */
    default:   return 0;
    }
}

static int translate(uint8_t sc)
{
    if (sc >= KEYMAP_SIZE)
        return 0;

    char base    = keymap[sc];
    char shifted = keymap_shift[sc];
    bool letter  = base >= 'a' && base <= 'z';

    char c;
    if (letter)
        c = (shift_down != caps_lock) ? shifted : base;
    else
        c = shift_down ? shifted : base;

    if (ctrl_down && letter)
        return base - 'a' + 1;        /* Ctrl+A .. Ctrl+Z -> 1..26 */
    return c;
}

static void keyboard_irq(registers_t *regs)
{
    (void)regs;
    uint8_t sc = inb(KBD_DATA_PORT);

    if (sc == 0xE0) {
        extended = true;
        return;
    }

    bool is_extended = extended;
    extended = false;

    bool released = sc & 0x80;
    sc &= 0x7F;

    if (is_extended) {
        if (sc == 0x1D)               /* right Ctrl */
            ctrl_down = !released;
        else if (!released) {
            int key = translate_extended(sc);
            if (key)
                push_key(key);
        }
        return;
    }

    switch (sc) {
    case 0x2A:
    case 0x36: shift_down = !released; return;
    case 0x1D: ctrl_down  = !released; return;
    case 0x3A: if (!released) caps_lock = !caps_lock; return;
    default: break;
    }

    if (!released) {
        int key = translate(sc);
        if (key)
            push_key(key);
    }
}

void keyboard_init(void)
{
    isr_register_handler(IRQ_BASE + 1, keyboard_irq);
    pic_unmask(1);
}

int keyboard_getkey(void)
{
    cpu_cli();
    while (head == tail) {
        /* "sti; hlt" is atomic with respect to interrupts: no lost wake-ups. */
        __asm__ volatile("sti; hlt" : : : "memory");
        cpu_cli();
    }
    int key = ring[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    cpu_sti();
    return key;
}
