/* Akira OS linker script: kernel is loaded at 1 MiB. */

ENTRY(_start)

SECTIONS
{
    . = 1M;
    kernel_start = .;

    /* The Multiboot header must be within the first 8 KiB of the image. */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    . = ALIGN(4K);
    kernel_end = .;
}
