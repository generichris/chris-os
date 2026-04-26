bits 32
extern kernel
global _start

_start:
    ; write C to prove kernel entry works 
    mov byte [0xB8004], 'C'
    mov byte [0xB8005], 0x0F

    mov esp, stack_top
    cld
    call kernel
    hlt

section .bss
align 4
stack_bottom:
    resb 16384
stack_top: