/* Built-in shell commands. Each returns 0 on success, non-zero on failure. */

#include "fs.h"
#include "heap.h"
#include "io.h"
#include "kernel.h"
#include "kprintf.h"
#include "kstring.h"
#include "notepad.h"
#include "paging.h"
#include "pmm.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"

/* ---------- general ---------- */

static int cmd_help(int argc, char **argv)
{
    if (argc > 1) {
        for (size_t i = 0; i < shell_command_count; i++) {
            if (strcmp(argv[1], shell_commands[i].name) == 0) {
                kprintf("usage: %s %s\n  %s\n", shell_commands[i].name,
                        shell_commands[i].usage, shell_commands[i].help);
                return 0;
            }
        }
        kprintf("help: no such command: %s\n", argv[1]);
        return 1;
    }

    kprintf("Available commands:\n");
    for (size_t i = 0; i < shell_command_count; i++)
        kprintf("  %-10s %s\n", shell_commands[i].name, shell_commands[i].help);
    kprintf("Tip: Up/Down browse history, PgUp/PgDn scroll back and forth,\n");
    kprintf("     Ctrl+L clears the screen.\n");
    return 0;
}

static int cmd_clear(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    vga_clear();
    return 0;
}

static int cmd_hello(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    kprintf("HI, I am Reinhard OS\n");
    return 0;
}

static int cmd_about(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    kprintf("Reinhard OS %s - a 32-bit x86 hobby kernel\n", REINHARD_VERSION);
    kprintf("  GDT/IDT/PIC interrupts, PIT timer, PS/2 keyboard\n");
    kprintf("  physical memory manager, paging with demand-paged heap\n");
    kprintf("  RAM filesystem, shell and notepad\n");
    return 0;
}

/* echo text...            print text
 * echo text... > file     overwrite file
 * echo text... >> file    append to file */
static int cmd_echo(int argc, char **argv)
{
    int redirect = -1;
    bool append = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            redirect = i;
            append = argv[i][1] == '>';
            break;
        }
    }

    int words_end = redirect < 0 ? argc : redirect;

    if (redirect < 0) {
        for (int i = 1; i < words_end; i++)
            kprintf("%s%s", i > 1 ? " " : "", argv[i]);
        kprintf("\n");
        return 0;
    }

    if (redirect + 1 >= argc) {
        kprintf("echo: missing file name after '%s'\n", argv[redirect]);
        return 1;
    }

    size_t total = 1;                                  /* trailing newline */
    for (int i = 1; i < words_end; i++)
        total += strlen(argv[i]) + 1;

    char *text = kmalloc(total + 1);
    if (!text) {
        kprintf("echo: out of memory\n");
        return 1;
    }
    size_t pos = 0;
    for (int i = 1; i < words_end; i++) {
        size_t n = strlen(argv[i]);
        memcpy(text + pos, argv[i], n);
        pos += n;
        if (i + 1 < words_end)
            text[pos++] = ' ';
    }
    text[pos++] = '\n';

    const char *path = argv[redirect + 1];
    fs_node_t *node = fs_lookup(path);
    fs_err_t err = FS_OK;
    if (!node)
        node = fs_create(path, FS_FILE, &err);
    if (node)
        err = append ? fs_append(node, text, pos) : fs_write(node, text, pos);

    kfree(text);
    if (err != FS_OK) {
        kprintf("echo: %s: %s\n", path, fs_strerror(err));
        return 1;
    }
    return 0;
}

/* ---------- filesystem ---------- */

static int cmd_ls(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : ".";
    fs_node_t *dir = fs_lookup(path);
    if (!dir) {
        kprintf("ls: cannot access '%s': %s\n", path, fs_strerror(FS_ERR_NOT_FOUND));
        return 1;
    }
    if (dir->type == FS_FILE) {
        kprintf("%-24s %u bytes\n", dir->name, (unsigned)dir->size);
        return 0;
    }
    if (!dir->first_child) {
        kprintf("(empty)\n");
        return 0;
    }

    uint8_t normal = vga_get_color();
    for (fs_node_t *c = dir->first_child; c; c = c->next_sibling) {
        if (c->type == FS_DIR) {
            vga_set_color(vga_color(VGA_LIGHT_CYAN, VGA_BLACK));
            kprintf("%-24s <dir>\n", c->name);
        } else {
            vga_set_color(normal);
            kprintf("%-24s %u bytes\n", c->name, (unsigned)c->size);
        }
    }
    vga_set_color(normal);
    return 0;
}

static int cmd_cd(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/";
    fs_node_t *dir = fs_lookup(path);
    if (!dir) {
        kprintf("cd: %s: %s\n", path, fs_strerror(FS_ERR_NOT_FOUND));
        return 1;
    }
    if (dir->type != FS_DIR) {
        kprintf("cd: %s: %s\n", path, fs_strerror(FS_ERR_NOT_DIR));
        return 1;
    }
    fs_set_cwd(dir);
    return 0;
}

static int cmd_pwd(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    char path[128];
    fs_path(fs_cwd(), path, sizeof(path));
    kprintf("%s\n", path);
    return 0;
}

static int make_node(const char *cmd, int argc, char **argv, fs_type_t type)
{
    if (argc < 2) {
        kprintf("usage: %s <path>\n", cmd);
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        fs_err_t err;
        if (type == FS_FILE && fs_lookup(argv[i]))
            continue;                                  /* touch on an existing file is a no-op */
        if (!fs_create(argv[i], type, &err)) {
            kprintf("%s: %s: %s\n", cmd, argv[i], fs_strerror(err));
            status = 1;
        }
    }
    return status;
}

static int cmd_mkdir(int argc, char **argv) { return make_node("mkdir", argc, argv, FS_DIR); }
static int cmd_touch(int argc, char **argv) { return make_node("touch", argc, argv, FS_FILE); }

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: cat <file>\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        fs_node_t *file = fs_lookup(argv[i]);
        if (!file) {
            kprintf("cat: %s: %s\n", argv[i], fs_strerror(FS_ERR_NOT_FOUND));
            status = 1;
        } else if (file->type != FS_FILE) {
            kprintf("cat: %s: %s\n", argv[i], fs_strerror(FS_ERR_IS_DIR));
            status = 1;
        } else {
            for (size_t k = 0; k < file->size; k++) {
                char c = (char)file->data[k];
                if (c == '\n' || c == '\t' || (c >= 32 && c < 127))
                    vga_putc(c);
            }
            if (file->size && file->data[file->size - 1] != '\n')
                vga_putc('\n');
        }
    }
    return status;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: rm <path>\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; i++) {
        fs_err_t err = fs_remove(argv[i]);
        if (err != FS_OK) {
            kprintf("rm: %s: %s\n", argv[i], fs_strerror(err));
            status = 1;
        }
    }
    return status;
}

static int cmd_notepad(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: notepad <file>\n");
        return 1;
    }
    return notepad_run(argv[1]);
}

/* ---------- system / memory ---------- */

static int cmd_meminfo(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);

    uint32_t total = pmm_total_frames() * (PAGE_SIZE / 1024);
    uint32_t used  = pmm_used_frames() * (PAGE_SIZE / 1024);
    heap_stats_t hs;
    heap_stats(&hs);

    kprintf("Physical memory : %u KiB total, %u KiB used, %u KiB free\n", total, used, total - used);
    kprintf("Kernel heap     : %u KiB reserved, %u bytes in use\n",
            (unsigned)(hs.mapped / 1024), (unsigned)hs.used);
    kprintf("Paging          : %u page faults, %u serviced by demand paging\n",
            paging_fault_count(), paging_demand_count());
    return 0;
}

static int cmd_uptime(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    uint32_t secs = timer_ticks() / timer_hz();
    kprintf("up %u:%02u:%02u (%u ticks @ %u Hz)\n",
            secs / 3600, (secs / 60) % 60, secs % 60, timer_ticks(), timer_hz());
    return 0;
}

static int cmd_pageinfo(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: pageinfo <virtual address>   e.g. pageinfo 0xB8000\n");
        return 1;
    }
    uint32_t addr;
    if (!parse_uint(argv[1], &addr)) {
        kprintf("pageinfo: bad address '%s'\n", argv[1]);
        return 1;
    }

    uint32_t pde, pte;
    bool mapped = paging_query(addr, &pde, &pte);

    kprintf("virtual %p  ->  directory[%u], table[%u], offset 0x%x\n",
            (void *)addr, addr >> 22, (addr >> 12) & 0x3FF, addr & 0xFFF);
    kprintf("  PDE = %08x (%s)\n", pde, (pde & PAGE_PRESENT) ? "present" : "not present");
    if (!mapped) {
        kprintf("  not mapped\n");
        return 0;
    }
    kprintf("  PTE = %08x  frame %p  [%s%s]\n", pte, (void *)(pte & ~(PAGE_SIZE - 1)),
            (pte & PAGE_WRITE) ? "writable" : "read-only",
            (pte & PAGE_USER) ? ", user" : ", kernel");
    kprintf("  physical address = %p\n", (void *)((pte & ~(PAGE_SIZE - 1)) | (addr & 0xFFF)));
    return 0;
}

/* Allocates N pages from the heap and touches each one, showing demand paging. */
static int cmd_vmtest(int argc, char **argv)
{
    uint32_t pages = 64;
    if (argc > 1 && (!parse_uint(argv[1], &pages) || pages == 0 || pages > 4096)) {
        kprintf("usage: vmtest [pages 1-4096]\n");
        return 1;
    }

    uint32_t faults_before = paging_demand_count();
    uint32_t frames_before = pmm_used_frames();

    uint8_t *mem = kmalloc((size_t)pages * PAGE_SIZE);
    if (!mem) {
        kprintf("vmtest: allocation failed\n");
        return 1;
    }
    kprintf("allocated %u KiB at %p (no frames committed yet)\n", pages * 4, mem);

    for (uint32_t i = 0; i < pages; i++)
        mem[(size_t)i * PAGE_SIZE + 16] = (uint8_t)(i ^ 0x5A);

    uint32_t bad = 0;
    for (uint32_t i = 0; i < pages; i++)
        if (mem[(size_t)i * PAGE_SIZE + 16] != (uint8_t)(i ^ 0x5A))
            bad++;

    kprintf("touched %u pages: %u demand-paging faults, %u new physical frames\n",
            pages, paging_demand_count() - faults_before, pmm_used_frames() - frames_before);
    kprintf("verification: %s\n", bad ? "FAILED" : "all pages read back correctly");

    kfree(mem);
    return bad ? 1 : 0;
}

static int cmd_crash(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    kprintf("Dereferencing NULL on purpose...\n");
    *(volatile uint32_t *)0 = 0xDEAD;
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    uint8_t status;
    do {
        status = inb(0x64);
    } while (status & 0x02);                           /* wait for the 8042 input buffer */
    outb(0x64, 0xFE);                                  /* pulse the CPU reset line */
    cpu_halt_forever();
}

static int cmd_shutdown(int argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);
    kprintf("Shutting down...\n");
    outw(0x604, 0x2000);                               /* QEMU (newer) */
    outw(0xB004, 0x2000);                              /* QEMU/Bochs (older) */
    kprintf("It is now safe to turn off your computer.\n");
    cpu_halt_forever();
}

/* ---------- command table ---------- */

const command_t shell_commands[] = {
    { "help",     "[command]",       "list commands or show usage",           cmd_help     },
    { "hello",    "",                "greet the user",                        cmd_hello    },
    { "about",    "",                "about Reinhard OS",                     cmd_about    },
    { "clear",    "",                "clear the screen",                      cmd_clear    },
    { "echo",     "text [> file]",   "print text, optionally to a file",      cmd_echo     },
    { "ls",       "[path]",          "list directory contents",               cmd_ls       },
    { "cd",       "[path]",          "change directory",                      cmd_cd       },
    { "pwd",      "",                "print working directory",               cmd_pwd      },
    { "mkdir",    "<path>...",       "create directories",                    cmd_mkdir    },
    { "touch",    "<file>...",       "create empty files",                    cmd_touch    },
    { "cat",      "<file>...",       "print file contents",                   cmd_cat      },
    { "rm",       "<path>...",       "remove files or empty directories",     cmd_rm       },
    { "notepad",  "<file>",          "edit a file (Ctrl+S save, Ctrl+Q quit)",cmd_notepad  },
    { "edit",     "<file>",          "alias for notepad",                     cmd_notepad  },
    { "meminfo",  "",                "physical, heap and paging statistics",  cmd_meminfo  },
    { "pageinfo", "<address>",       "show the page-table entries for an address", cmd_pageinfo },
    { "vmtest",   "[pages]",         "demonstrate demand paging",             cmd_vmtest   },
    { "uptime",   "",                "time since boot",                       cmd_uptime   },
    { "crash",    "",                "trigger a page fault (NULL write)",     cmd_crash    },
    { "reboot",   "",                "restart the machine",                   cmd_reboot   },
    { "shutdown", "",                "power off (QEMU)",                      cmd_shutdown },
};

const size_t shell_command_count = ARRAY_SIZE(shell_commands);
