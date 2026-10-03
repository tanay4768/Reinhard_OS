/* Low-level interrupt entry points.
 *
 * Every vector gets a tiny stub that normalises the stack (pushing a dummy
 * error code when the CPU does not) and then jumps to isr_common, which saves
 * state and calls the C dispatcher isr_handler(registers_t *).
 *
 * isr_stub_table[] (in .data) holds the address of each stub, in vector order.
 */

.section .data
.global isr_stub_table
isr_stub_table:

.macro ISR_NOERR n
.section .text
.global isr\n
isr\n:
    push $0
    push $\n
    jmp  isr_common
.section .data
    .long isr\n
.endm

.macro ISR_ERR n
.section .text
.global isr\n
isr\n:
    push $\n
    jmp  isr_common
.section .data
    .long isr\n
.endm

/* CPU exceptions 0-31 (8, 10-14, 17, 21, 29, 30 push an error code) */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

/* Hardware IRQs 0-15, remapped to vectors 32-47 */
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

.section .text
isr_common:
    pusha
    mov  %ds, %eax
    push %eax                      /* save data segment */
    mov  $0x10, %ax                /* kernel data selector */
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    cld
    push %esp                      /* registers_t * */
    call isr_handler
    add  $4, %esp
    pop  %eax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    popa
    add  $8, %esp                  /* drop int_no + err_code */
    iret

.section .note.GNU-stack,"",@progbits
