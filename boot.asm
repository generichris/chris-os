bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Set VESA mode 0x118 (1024x768x32) with linear framebuffer
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10

    ; Whether that worked or not, hardcode QEMU's known framebuffer address.
    ; qemu -vga std always maps LFB at 0xFD000000 for mode 0x118.
    ; If VESA call failed, addr stays 0 and kernel falls back to text.
    cmp ax, 0x004F
    jne .no_vesa

    mov dword [0x500], 0xFD000000   ; framebuffer address
    mov dword [0x504], 1024          ; width
    mov dword [0x508], 768           ; height
    mov dword [0x50C], 32            ; bpp
    mov dword [0x510], 4096          ; pitch (1024 * 4)
    jmp .load

.no_vesa:
    mov dword [0x500], 0
    mov dword [0x504], 0
    mov dword [0x508], 0
    mov dword [0x50C], 0
    mov dword [0x510], 0

.load:
    mov si, disk_packet
    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    jc .err

    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm

.err:
    mov si, errmsg
.eloop:
    lodsb
    or al, al
    jz .ehalt
    mov ah, 0x0E
    int 0x10
    jmp .eloop
.ehalt:
    hlt
    jmp .ehalt

errmsg      db 'Disk error!', 0

disk_packet:
    db 0x10, 0
    dw 128
    dw 0x0000
    dw 0x1000
    dq 1

gdt_start:
    dq 0
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

bits 32
pm:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000

times 510-($-$$) db 0
dw 0xAA55