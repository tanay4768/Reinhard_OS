# Akira OS build system.
#
#   make          build build/akira_os.bin
#   make run      boot it in QEMU (direct multiboot, no ISO needed)
#   make iso      build akira_os.iso with GRUB
#   make run-iso  boot the ISO in QEMU
#   make clean
#
# Uses an i686-elf cross-compiler when available; otherwise falls back to the
# host gcc in 32-bit freestanding mode.

ifeq ($(shell command -v i686-elf-gcc 2>/dev/null),)
  CC := gcc -m32 -fno-pie -fno-stack-protector -fcf-protection=none
  LD := ld -m elf_i386
else
  CC := i686-elf-gcc
  LD := i686-elf-ld
endif

CFLAGS  := -std=gnu11 -ffreestanding -O2 -g -Wall -Wextra -Iinclude \
           -fno-tree-loop-distribute-patterns
LDFLAGS := -T linker.ld -nostdlib -z noexecstack

SRC_C := $(shell find src -name '*.c')
SRC_S := $(shell find src -name '*.s')
OBJS  := $(patsubst src/%.c,build/%.o,$(SRC_C)) $(patsubst src/%.s,build/%.o,$(SRC_S))

KERNEL := build/akira_os.bin
ISO    := akira_os.iso

.PHONY: all run run-iso iso clean

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@grub-file --is-x86-multiboot $@ 2>/dev/null && echo "multiboot header OK" || true

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

iso: $(KERNEL) grub.cfg
	mkdir -p build/iso/boot/grub
	cp $(KERNEL) build/iso/boot/akira_os.bin
	cp grub.cfg build/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) build/iso

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 128M

run-iso: iso
	qemu-system-i386 -cdrom $(ISO) -m 128M

clean:
	rm -rf build $(ISO)
