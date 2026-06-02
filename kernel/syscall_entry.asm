bits 32

extern syscall_handler
global syscall_entry

syscall_entry:
    push byte 0
    push byte 0x80
    pusha
    push esp
    call syscall_handler
    add esp, 4
    popa
    add esp, 8
    iret
