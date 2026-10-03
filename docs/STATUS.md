# Reinhard OS — Current Status

Baseline: v0.2.0 (32-bit x86, C + assembly, Multiboot, boots under QEMU).

> **Evidence levels.** Each claim is tagged:
> **[R]** stated in `Readme.md`, **[M]** read from the `Makefile`, **[S]** seen in the QEMU
> screenshot of a boot, **[?]** inferred and *must be verified against `src/`*.
> The first pass of this document was written without access to `src/` (GitHub blocked
> automated tree browsing), so anything marked [?] is a hypothesis, not a finding.
> Run the checklist at the bottom to turn every [?] into a fact.
>
> **Update.** KI-1 to KI-4 are now resolved against the real sources, and the
> result was booted under QEMU: RAMFS, demand-paged heap, paging diagnostics, the
> shell and the PgUp/PgDn scrollback were all exercised on the rebuilt kernel.

---

## 1. What exists today

| Area | Implemented | Evidence |
|---|---|---|
| Boot | Multiboot header, 16 KiB boot stack, loads via GRUB or `qemu -kernel` | R |
| CPU setup | Flat GDT, IDT with 32 exception vectors + 16 IRQs, PIC remap, spurious-IRQ handling | R, S |
| Drivers | VGA text (scroll, 200-line scrollback on PgUp/PgDn with any other key returning to the live screen, cursor, colours), PIT @ 100 Hz, PS/2 keyboard (shift, caps, ctrl, arrows, Home/End/Del/PgUp/PgDn) | R, S |
| Physical memory | Bitmap frame allocator fed by the Multiboot memory map; low memory + kernel image reserved | R |
| Virtual memory | Two-level 32-bit paging, recursive page-directory mapping (PD at `0xFFFFF000`, PT *N* at `0xFFC00000 + N*4096`), NULL-page guard, first 8 MiB identity-mapped | R |
| Kernel heap | Demand-paged: `heap_grow()` only moves `brk`; page-fault handler maps frames on first touch. First-fit allocator with split/coalesce | R |
| Filesystem | Hierarchical RAM filesystem: files, directories, `.`/`..`, absolute/relative paths | R |
| Shell | History, quoting, `>` / `>>` for `echo`; commands: `help about hello clear echo ls cd pwd mkdir touch cat rm notepad/edit meminfo pageinfo vmtest uptime crash reboot shutdown` | R |
| Apps | Full-screen Notepad (Ctrl+S save, Ctrl+Q quit) | R |
| Diagnostics | Page-fault decoder + register dump + "System halted"; `crash`, `vmtest`, `pageinfo` as built-in exercisers | R, S |
| Build | `make`, `make run`, `make iso`, `make run-iso`, i686-elf cross-compiler or host `gcc -m32` fallback | M |

## 2. Architecture (as documented)

```
            +------------------------------------------------------+
            |  shell (src/shell)        notepad (src/apps)         |   <- kernel-mode "apps"
            +------------------------------------------------------+
            |  ramfs (src/fs)           kprintf/string (src/lib)   |
            +------------------------------------------------------+
            |  pmm -> paging -> heap (src/mm)                      |
            +------------------------------------------------------+
            |  vga | timer | keyboard (src/drivers)                |
            +------------------------------------------------------+
            |  gdt | idt | pic | isr dispatcher | asm stubs (arch)  |
            +------------------------------------------------------+
            |  multiboot header + _start (src/boot)                |
            +------------------------------------------------------+
```

Everything runs in ring 0, in one address space, in one flow of control driven by interrupts.
The shell and Notepad are ordinary kernel functions, not processes.

Memory map in use today:

| Range | Use | Evidence |
|---|---|---|
| `0x00000000` page | Unmapped NULL guard | R |
| `0x00100000`+ | Kernel image (faulting `eip` was `0x0010255a`) | S, R |
| first 8 MiB | Identity-mapped | R |
| `0xD0000000` – `0xD8000000` | Demand-paged kernel heap (`KHEAP_START`/`KHEAP_MAX`, 128 MiB of address space; the bound matches the `edi=0xd8000000` in the original panic dump) | R, S |
| `0xFFC00000` – `0xFFFFFFFF` | Recursive page-table window | R |

Design decisions already made that the roadmap keeps: recursive paging, demand paging, no libgcc
dependency (no 64-bit division in the kernel), `-fno-tree-loop-distribute-patterns` for `string.c`.

## 3. Known issues (fix before building anything new)

| ID | Issue | Evidence | Status |
|---|---|---|---|
| KI-1 | **Boot panic:** page fault at `0xD0000000` (write, not present, kernel mode) after "Timer and keyboard drivers OK". That address is the heap base; the demand-paging handler declined to map it. Registers (`ecx=0xff0`, `esi=0xd0001000`, `edi=0xd8000000`) look like the first free-block header (one page minus a 16-byte header) being written during the first allocation. | S | **Fixed.** Root cause: at `-O2` GCC sank the `brk` store *below* the first write into the new heap page, so the fault handler still saw the old `brk` and refused to map `0xD0000000` (`eip=0x0010286a`, faulting instruction `mov %ecx,(%eax)`). The store is now published before the page is touched (`volatile brk` + compiler barrier) and the fault handler authorises the fixed `KHEAP_START`–`KHEAP_MAX` range instead of consulting `brk` (`heap_reserved()`). |
| KI-2 | Makefile name `build/reinhard _os.bin` contains a space, so make builds two targets and QEMU receives a stray `_os.bin` as a disk image (the "Image format was not specified" warning). | M | Fixed in the attached Makefile. |
| KI-3 | Object files did not depend on headers; header changes did not rebuild users. | M | Fixed (`-MMD -MP`). |
| KI-4 | Name drift (Reinhard → Akira → Reinhard) left mixed references. | — | Fixed: `scripts/rename-to-reinhard.sh` has been run over the tree (`prompts/` is skipped on purpose, it documents the old names). |

## 4. Gaps against the long-term goals

| Goal | What is missing |
|---|---|
| Multitasking | Task structure, context switch, scheduler, blocking/wake primitives, IRQ-safe locking. The timer fires but drives only `uptime`. |
| User mode | TSS, ring-3 segments, per-process page directories, user/kernel address split, `copy_from_user`. The kernel image sits in low memory, where user code would normally live. |
| System calls | Entry mechanism, syscall table, file-descriptor table. |
| Executables | ELF loader, initrd/boot-module support, a userspace C library. |
| Storage | PCI enumeration, block driver (ATA/virtio-blk), VFS abstraction, a persistent filesystem. Nothing survives a reboot. |
| Networking | PCI, NIC driver, every protocol layer, sockets, wait queues. |
| Security | No isolation (everything is ring 0), no permissions, and **non-PAE 32-bit paging has no NX bit**, so W^X needs PAE or long mode. |
| Linux compatibility | Needs user mode, ELF, threads/futex, signals, a large syscall surface. Most modern Linux software is x86-64, which a 32-bit kernel cannot run. |
| Graphics | VGA text only; no framebuffer, input beyond keyboard, or compositor. |
| Quality | No automated tests, no CI, no serial/debug console, no backtraces, no assertions. |

## 5. Verification checklist (turns [?] into facts)

Run from the repo root and compare against the tables above:

```sh
# 1. Inventory of source files and size
find src include -type f \( -name '*.[chs]' -o -name '*.S' \) | xargs wc -l | sort -n

# 2. Heap and paging: the code behind KI-1
sed -n '1,200p' src/mm/heap.c
grep -n "page_fault\|heap_brk\|HEAP_START\|HEAP_END\|HEAP_MAX" -r src include

# 3. Which modules are actually wired into boot, and in what order
grep -n "init" src/kernel*.c src/boot/*.c 2>/dev/null | head -50

# 4. Warnings the compiler already reports
make clean && make 2>&1 | grep -i warning | sort | uniq -c
```
