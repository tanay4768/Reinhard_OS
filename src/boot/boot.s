/* Multiboot header + entry point. GRUB (or QEMU -kernel) jumps to _start. */

.set ALIGN,    1<<0               /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1               /* ask the bootloader for a memory map     */
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384                        /* 16 KiB boot stack */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov  $stack_top, %esp
    xor  %ebp, %ebp
    and  $-16, %esp                /* keep the ABI's 16-byte alignment at the call */
    sub  $8, %esp
    push %ebx                      /* arg 2: multiboot_info_t *             */
    push %eax                      /* arg 1: bootloader magic               */
    call kernel_main
    cli
1:  hlt
    jmp  1b
.size _start, . - _start

.section .note.GNU-stack,"",@progbits
