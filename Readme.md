# Reinhard  OS

A small 32-bit x86 operating system written in C and assembly.

## Features

| Area | What's implemented |
|------|--------------------|
| Boot | Multiboot (GRUB or `qemu -kernel`), 16 KiB boot stack |
| CPU | GDT (flat model), IDT with all 32 exception vectors + 16 IRQs, PIC remap, spurious-IRQ handling |
| Drivers | VGA text mode (scrolling, hardware cursor, colours), PIT timer @ 100 Hz, PS/2 keyboard (shift, caps lock, ctrl, arrows, Home/End/Del/PgUp/PgDn) |
| Memory | Bitmap **physical memory manager**, two-level **paging** with recursive page-directory mapping, NULL-page guard, **demand-paged kernel heap**, first-fit allocator with split/coalesce |
| Filesystem | Hierarchical RAM filesystem (files, directories, `.`/`..`, absolute/relative paths) |
| Apps | **Notepad** full-screen editor; shell with history, quoting and `>` / `>>` redirection for `echo` |

## Layout

```
src/boot/      multiboot header + _start
src/arch/      gdt, idt, pic, isr dispatcher, interrupt stubs (asm)
src/drivers/   vga, timer, keyboard
src/mm/        pmm (frames), paging (page tables, fault handler), heap (kmalloc)
src/fs/        ramfs
src/lib/       string routines, kprintf/ksnprintf
src/shell/     shell loop + built-in commands
src/apps/      notepad
include/       one header per module
```

## Build and run

Requirements: `i686-elf-gcc` (recommended) **or** a 32-bit-capable host `gcc` + `ld`, plus `make` and QEMU.
For ISOs also `grub-mkrescue` and `xorriso`.

```
make          # build/reinhard _os.bin
make run      # boot straight in QEMU (multiboot, no ISO)
make iso      # reinhard _os.iso via GRUB
make run-iso
make clean
```

## Shell commands

`help` `about` `hello` `clear` `echo` `ls` `cd` `pwd` `mkdir` `touch` `cat` `rm`
`notepad`/`edit` `meminfo` `pageinfo` `vmtest` `uptime` `crash` `reboot` `shutdown`

Notepad keys: arrows, Home/End, PgUp/PgDn, Backspace/Delete, Tab (4 spaces),
**Ctrl+S** save, **Ctrl+Q** quit (press twice to discard unsaved changes).

Try these to see paging in action:

```
vmtest 256            # touch 256 untouched heap pages -> 256 demand-paging faults
pageinfo 0xB8000      # walk the page tables for the VGA buffer
pageinfo 0xD0000000   # first heap page
crash                 # NULL write -> page-fault panic with register dump
```

## Design notes

* **Recursive paging.** The last page-directory entry points at the directory itself, so
  the directory lives at `0xFFFFF000` and page table *N* at `0xFFC00000 + N*4096`. Mappings
  can be edited for any frame without identity-mapping all of RAM.
* **Demand paging.** `heap_grow()` only moves the `brk` pointer. The first touch of a page
  raises a page fault; the handler allocates a frame, maps it, zeroes it and resumes.
  Faults outside the heap (or protection faults) panic with a decoded error code.
* **Memory map.** The PMM reads the Multiboot memory map; low memory and the kernel image
  (`kernel_end` from `linker.ld`) are reserved. The first 8 MiB are identity-mapped.
* **No libgcc.** The code avoids 64-bit division, so the link needs nothing beyond the kernel.
* `string.c` is built with `-fno-tree-loop-distribute-patterns`; otherwise GCC may compile
  `memset` into a call to itself.

## Roadmap

Preemptive multitasking and a scheduler, user mode (ring 3 + TSS + syscalls), ELF loader,
ATA/disk driver with a persistent filesystem, then networking (the original TCP/IP goal).
