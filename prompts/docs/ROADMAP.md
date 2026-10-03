# Reinhard OS — Roadmap

Goal: an independent, terminal-first OS on our own kernel, growing from today's v0.2.0 (see `STATUS.md`).

## Ground rules

1. **Never regress.** Every shell command, Notepad, `vmtest`, `pageinfo` and `crash` becomes a regression test in Phase 0 and keeps passing in every later phase.
2. **One milestone = one tag.** Each milestone ends with a green `make test`, a tagged release and an updated `STATUS.md`.
3. **Dependencies first.** A phase starts only when everything it depends on has a passing milestone. Work inside a phase can run in parallel where marked.
4. **Decide, then build.** Architecture decisions are recorded as short ADRs in `docs/adr/` at the gates below.
5. **Own the kernel.** Linux compatibility is a translation layer on top (Phase 8); the kernel's native ABI stays Reinhard's.

## Dependency graph

```
P0 Stabilise ─► P1 Foundations ─► P2 Multitasking ─► P3 User mode ─► P4 Storage/VFS/TTY ─┬► P6 Native platform ─► P7 Security
                    │                    │                                               │            │
                    │                    └────────── P5a/b kernel-mode networking ◄──────┤            └► P8 Linux ABI ─► P9 Graphics/desktop
                    └─ ADR-001 (32/64-bit, PAE)                                          └► P5c/d sockets, TCP, tools (needs P3)
```

---

## Phase 0 — Stabilise and make it testable

**Why first:** you cannot safely build on a kernel that panics at boot and has no tests.

| Task | Detail |
|---|---|
| Fix KI-1 | Resolve the heap-base page fault (see `STATUS.md`): set `brk` before touching new memory, make the handler's heap-range check authoritative, create and zero the page table via the recursive window when the directory entry is absent. |
| Apply rename | Run `scripts/rename-to-reinhard.sh`; regenerate the banner; use the new Makefile. |
| Serial console | Mirror `kprintf` to COM1 so QEMU can log with `-serial stdio` / `-serial file:`. |
| Test harness | `make test`: boots `qemu-system-i386 -display none -serial file:test.log -device isa-debug-exit,iobase=0xf4,iosize=0x04 -no-reboot`, runs in-kernel self-tests (`ktest`), exits with a pass/fail code. |
| Host unit tests | Pure code (string routines, `ksnprintf`, ramfs path logic, allocator logic) compiles against the host and runs in `make unit`. |
| Kernel hygiene | `kassert`, panic with symbolised backtrace (frame pointers + `nm` table), `-Werror` for new code. |
| CI | GitHub Actions: build with the cross-compiler, run `make unit test`. |

**Milestone M0 — "boots green" (tag v0.2.1):** `make test` passes on a clean checkout and in CI. Self-tests cover: heap alloc/free/coalesce, `vmtest 256` produces 256 demand faults, `pageinfo 0xB8000` walks correctly, a NULL write panics with the expected code, ramfs create/read/delete, shell redirection.

---

## Phase 1 — Kernel foundations

| Task | Detail |
|---|---|
| **ADR-001: 32-bit vs x86-64** | Decide now. Reasons to go 64-bit: modern Linux binaries are x86-64, NX bit, large address space, a long-lived design. Reasons to stay 32-bit: simpler, matches current code, a Linux ABI limited to i386 programs. A middle path is 32-bit **PAE** (adds NX, 64-bit PTEs). Whatever is chosen, do it **here**, before user mode, because paging, syscalls and the loader all depend on it. |
| Higher-half kernel | Link the kernel at the top of the address space and keep user space low. This gives the standard user/kernel split that Phase 3 needs. Today the kernel image is at 1 MiB and the first 8 MiB are identity-mapped, which collides with typical user addresses. |
| Temporary mappings | `kmap/kunmap` (or a physical-memory window) so the kernel can touch any frame, not only the first 8 MiB. Required to build page tables for new processes. |
| Interrupt-safe primitives | `irq_save/irq_restore`, spinlock API (a no-op on uniprocessor, but used everywhere from now on). |
| PMM improvements | Frame refcounts (for copy-on-write later), allocation stats, optional per-zone free lists. |
| Early kernel log | Ring-buffer log with levels, readable from the shell (`dmesg`). |

**Milestone M1 — "stable memory" (v0.3.0):** all M0 tests pass on the higher-half kernel; a stress test allocates and frees across all of RAM without leaks (frame count returns to baseline); `meminfo` and `dmesg` reflect reality.

---

## Phase 2 — Multitasking (kernel threads)

| Task | Detail |
|---|---|
| Task control block | Kernel stack per thread, state, saved registers, priority, owning-process pointer (null for now). |
| Context switch | Assembly switch routine; new-thread trampoline; idle thread. |
| Scheduler | Round-robin driven by the 100 Hz PIT; `yield`, `sleep(ms)`, `exit`. |
| Blocking primitives | Wait queues, mutex, semaphore, condition variable. Keyboard input blocks the reader instead of busy-polling. |
| Shell as a thread | The shell and Notepad become tasks; `ps`, `kill` (kernel threads) and `top`-like view. |

**Milestone M2 — "two things at once" (v0.4.0):** two background threads print interleaved while the shell stays responsive; a blocked thread consumes no CPU (idle count rises); deadlock-free producer/consumer test under 10 000 iterations; all M0/M1 tests still pass.

---

## Phase 3 — User mode, system calls, executables

| Task | Detail |
|---|---|
| TSS and ring 3 | User code/data segments, TSS with kernel stack switching. |
| Address spaces | Per-process page directory, clone of kernel mappings, teardown that returns every frame. |
| Syscall interface | `int 0x80` first (works with the loader and, later, the Linux i386 layer); fast path (`sysenter`/`syscall`) later. Table-driven dispatch. |
| User-pointer safety | `copy_from_user`, `copy_to_user`, `strncpy_from_user`, with fault recovery so a bad pointer returns `-EFAULT` instead of panicking. |
| File descriptors | Per-process fd table; `open read write close lseek stat getdents`. |
| VFS layer | Insert a VFS between callers and ramfs (inode/dentry-style operations) so later filesystems plug in. |
| Process lifecycle | `fork`/`exec`/`wait`/`exit` (or `spawn`), parent/child, zombie reaping, copy-on-write fork once refcounts exist. |
| Executable loading | ELF loader; initrd delivered as a Multiboot module (GRUB `module`, QEMU `-initrd`). |
| Minimal libc | `crt0`, syscall stubs, `printf`, `malloc` via `brk`/`mmap`. |
| Userspace shell | Port the shell to ring 3, keeping the same command behaviour. |

**Milestone M3a — "hello from ring 3" (v0.5.0):** a user ELF from the initrd prints via `write` and exits with a status that the parent reads.
**Milestone M3b — "isolation holds" (v0.6.0):** a user process that dereferences NULL, executes a privileged instruction, or passes a bad pointer is killed and the kernel and shell survive; a fork loop of 200 processes ends with frame count at baseline; the shell runs entirely in user mode.

---

## Phase 4 — Storage, VFS completion, TTY

| Task | Detail |
|---|---|
| PCI | Bus enumeration and a device registry (also needed by networking). |
| Block layer | ATA PIO driver first (simplest under QEMU), `virtio-blk` later; sector cache. |
| Persistent filesystem | FAT32 (interoperable, easy to inspect) or ext2 (richer permissions); decide in an ADR. Read/write, mount/unmount. |
| devfs and TTY | `/dev/console`, `/dev/null`, `/dev/zero`; line discipline, raw/cooked modes, job-control groundwork. |
| Pipes and redirection | `pipe`, `dup2`, shell `|`, `<`, `>`, `>>` for any program, not only `echo`. |
| Core utilities | `cp mv cat ls rm mkdir grep wc head tail sort` as user programs. |

**Milestone M4 — "survives reboot" (v0.7.0):** write a file, reboot QEMU with the same disk image, read it back; `ls | grep | wc` works; the filesystem passes a corruption/unclean-shutdown test (`fsck` on the image reports clean).

---

## Phase 5 — Networking *(starts after M2 for kernel-mode work; user-visible sockets need M3/M4)*

Reuse the dependencies: PCI (P4), wait queues (P2), timers (have).

| Sub-phase | Deliverables | Milestone |
|---|---|---|
| **5a Link** | NIC driver (`virtio-net` or `e1000` for QEMU, behind a NIC interface), Ethernet framing, ARP cache, packet-buffer (`skb`-like) pool | Reply to `arping` from the host; packets visible in a QEMU `filter-dump` pcap |
| **5b IP** | IPv4 (header checks, fragmentation reassembly), ICMP echo, static config | `ping` works both directions with QEMU user networking and a tap device |
| **5c UDP and services** | UDP, DHCP client, DNS stub resolver, BSD-style socket layer (`socket bind sendto recvfrom`) | `dhcp` obtains a lease; `nslookup example.com` resolves |
| **5d TCP** | State machine, retransmission with RTT estimation, windowing, congestion control (start with Reno), `listen/accept/connect`, `SO_REUSEADDR`, timeouts | HTTP GET from a guest program; a guest echo server handles 100 sequential and 10 concurrent connections; survives 5 % packet loss injected with `tc netem` |
| **5e Routing and tools** | Routing table (longest prefix match), multiple interfaces, loopback, `ifconfig`/`ip`, `route`, `ping`, `netstat`, `nc`, `dig`, `wget` | Static route, default gateway and loopback tests pass in `make test-net` |

Later: IPv6, firewall hooks (needed by Phase 7), TLS (port BearSSL).

---

## Phase 6 — Native application platform

| Task | Detail |
|---|---|
| Executable format | **Use ELF** with a Reinhard `PT_NOTE` manifest (application ID, version, requested permissions, minimum ABI). This keeps compilers and debuggers working and lets Phase 8 share the loader. |
| Syscall ABI | Freeze and version it (`docs/abi.md`); never break it without a version bump. |
| SDK | `libr`: libc (consider porting musl), `crt0`, linker script, headers; a `reinhard-gcc` wrapper now and a proper `--target=i686-reinhard` toolchain later; examples and a template project. |
| Package format | `.rpk` archive: manifest, file list with hashes, signature, install/remove scripts. |
| Package manager | `rpk install/remove/list/info/verify`, package database on disk; local files first, then a repository index over HTTP (needs Phase 5d). |
| Dynamic linking | Optional: shared libraries and an in-OS dynamic loader. |

**Milestone M6 — "install an app" (v0.9.0):** build an app with the SDK on the host, package it, install it in the guest with `rpk`, run it, remove it cleanly; reinstall and verify file hashes.

---

## Phase 7 — Security model *(basic isolation already arrives in M3b; this phase makes it a policy)*

| Task | Detail |
|---|---|
| Users and ownership | uid/gid, file permission bits enforced in the VFS. |
| Application permissions | Manifest-declared capabilities (network, filesystem scopes, devices, spawn) enforced at syscall entry. |
| Hardening | NX/W^X (needs PAE or 64-bit, per ADR-001), SMEP/SMAP where available, ASLR for stack/heap/mmap, stack guard pages, syscall argument validation fuzzing. |
| Secure install | Signed packages (Ed25519 + SHA-256; vendor a small public-domain library such as Monocypher), trusted-key store, rollback protection, install-time permission prompt. |
| Auditing | Kernel audit log for denied operations. |

**Milestone M7 — "denied" (v1.0):** an unsigned or tampered package is rejected; an app without the `net` permission gets `-EPERM` from `socket`; a stack-smash test crashes the process rather than executing data; a syscall fuzzer runs 10 minutes without a kernel panic.

---

## Phase 8 — Linux ABI compatibility layer

A per-process *personality*: Linux processes use a Linux syscall table and struct layouts; Reinhard processes are unaffected. The kernel remains ours (compare FreeBSD's linuxulator or WSL1).

| Stage | Scope | Milestone |
|---|---|---|
| 8a | Linux ELF recognition; i386 syscall translation for static musl binaries (`write read open close brk mmap munmap exit_group ioctl(TCGETS) uname getpid clock_gettime` …) | Static busybox `echo`/`ls`/`cat` run |
| 8b | Threads and sync: `clone`, `futex`, TLS (`set_thread_area`), `sigaction`/signals, `wait4`, `getdents64`, `/proc/self`, `/dev` | A multithreaded static program passes; busybox `sh` runs scripts |
| 8c | Dynamic loading: `PT_INTERP`, musl then glibc dynamic binaries, `mmap` file-backed and `mprotect` | Dynamic coreutils, ncurses applications run |
| 8d | Networking and event APIs: sockets mapping, `epoll`, `eventfd`, `timerfd`, `poll` | Curl and a small HTTP server run |
| 8e | Desktop-class and games (needs Phase 9): framebuffer/DRM subset, input devices, audio, a Wayland or X11 path | A simple SDL game renders |

**Reality check:** if ADR-001 keeps the kernel 32-bit, this layer runs i386 Linux programs only. Most current games are x86-64, so 8e effectively requires the 64-bit decision. Games also need GPU acceleration, audio and a large, accurately implemented syscall surface, so treat 8e as the long-tail stretch goal.

---

## Phase 9 — Graphics and desktop

| Task | Detail |
|---|---|
| Framebuffer console | Request a linear framebuffer through Multiboot, bitmap-font terminal, keeps the VGA text console as a fallback. |
| Input | PS/2 mouse, input event queue. |
| Display server | Kernel mode-set (Bochs/virtio-gpu in QEMU), a user-space compositor with a window protocol, shared-memory buffers. |
| Window manager | Stacking WM, focus, decorations, terminal emulator as the first client. |
| Toolkit and apps | Small widget toolkit, file manager, text editor (successor of Notepad), settings. |
| Linux graphics | Bridge for Linux GUI clients once Phase 8e exists. |

**Milestone M9 — "first window" (v1.x):** the terminal runs as a windowed application alongside a second client, driven by keyboard and mouse.

---

## Suggested first four weeks

1. **Day 1–2:** apply the Makefile and rename; fix KI-1; tag **v0.2.1**.
2. **Week 1:** serial logging, `ktest` plus `make test`, CI; reach **M0**.
3. **Week 2–3:** ADR-001, higher-half kernel, temporary mappings, locks; reach **M1**.
4. **Week 4:** task structure, context switch, round-robin scheduler; aim at **M2**.

## Risks to manage

| Risk | Mitigation |
|---|---|
| Rewriting memory layout twice (32→64-bit later) | Decide ADR-001 in Phase 1, before user mode exists |
| TCP complexity | Build it only after UDP/DHCP/DNS are stable; test with packet loss from day one |
| Scope creep | Each phase has one milestone and a test; do not start the next before it is green |
| Linux compatibility expectations | Staged milestones; claim only what the tests prove |
| No source review yet | Run the verification checklist in `STATUS.md` and correct the [?] items first |
