#ifndef AKIRA_MULTIBOOT_H
#define AKIRA_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_FLAG_MEM  (1u << 0)
#define MULTIBOOT_FLAG_MMAP (1u << 6)
#define MULTIBOOT_MMAP_AVAILABLE 1u

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;   /* KiB below 1 MiB */
    uint32_t mem_upper;   /* KiB above 1 MiB */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;        /* size of the rest of the entry (excludes this field) */
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif
