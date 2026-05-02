bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    ; Query VESA mode info for mode 0x118 into buffer at 0x600
    ; ES=0 already (set above), DI=offset in segment
    mov ax, 0x4F01
    mov cx, 0x0118
    mov di, 0x0600
    int 0x10
    cmp ax, 0x004F
    jne .no_vesa            ; mode not supported — fall back

    ; Read ALL fields from ModeInfoBlock NOW, before 0x4F02 can clobber ES
    ; Restore DS=0 just in case BIOS touched it
    xor ax, ax
    mov ds, ax

    ; ModeInfoBlock offsets (VBE 2.0):
    ;   0x10  word  BytesPerScanLine
    ;   0x12  word  XResolution
    ;   0x14  word  YResolution
    ;   0x19  byte  BitsPerPixel
    ;   0x28  dword PhysBasePtr
    movzx eax, word [0x0610]   ; BytesPerScanLine -> pitch
    mov dword [0x510], eax

    movzx eax, word [0x0612]   ; XResolution -> width
    mov dword [0x504], eax

    movzx eax, word [0x0614]   ; YResolution -> height
    mov dword [0x508], eax

    movzx eax, byte [0x0619]   ; BitsPerPixel -> bpp
    mov dword [0x50C], eax

    mov eax, dword [0x0628]    ; PhysBasePtr -> fb_addr
    test eax, eax
    jnz .have_addr
    mov eax, 0xFD000000        ; fallback if BIOS left it 0
.have_addr:
    mov dword [0x500], eax

    ; NOW set the mode (bit 14 = use linear framebuffer)
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne .no_vesa
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
    mov dl, [boot_drive]
    int 0x13
    jc .err

    in al, 0x92
    or al, 2
    and al, 0xFE
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
boot_drive  db 0

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