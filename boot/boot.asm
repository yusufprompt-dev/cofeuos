[org 0x7c00]
KERNEL_OFFSET equ 0x10000   ; Kernel fiziksel adres (64KB - bootloader ile çakışmaz)
VBE_MODE_INFO equ 0x8000    ; VBE bilgisi için güvenli adres
VBE_MODE equ 0x118

xor ax, ax
cli
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7000
sti
mov [BOOT_DRIVE], dl

; Stack Ayarla
mov bp, 0x7000

; --- HATA AYIKLAMA 1: Başladık ---
mov al, 'B' ; 'B'ooting...
call debug_char

; Kernel'ı yükle
call load_kernel

; --- HATA AYIKLAMA 2: Kernel Yüklendi ---
mov al, 'K' ; 'K'ernel Loaded...
call debug_char

; VESA lineer framebuffer moduna geç.
mov al, 'V'
call debug_char
call set_vesa_mode

mov al, 'P'
call debug_char
call switch_to_pm
jmp $

; ... (load_kernel, switch_to_pm ve GDT kısımları bir önceki mesajdakiyle aynı kalsın) ...


[bits 16]
load_kernel:
    mov ah, 0x00
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    mov al, 'R'
    call debug_char

    mov ah, 0x08
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    and cl, 0x3f
    cmp cl, 63
    je .spt63
    cmp cl, 49
    je .spt49
    cmp cl, 18
    je .spt18
    mov al, '?'
    call debug_char
    jmp .after_spt
.spt63:
    mov al, 'S'
    call debug_char
    jmp .after_spt
.spt49:
    mov al, 'N'
    call debug_char
    jmp .after_spt
.spt18:
    mov al, 'F'
    call debug_char
.after_spt:

    ; Kernel'i 0x10000 fiziksel adresine yükle
    ; ES:0000 = 0x1000 segmenti → fiziksel 0x10000
    mov ax, 0x1000
    mov es, ax

    ; Chunk 1: fiziksel 0x10000, sektör 2, 8 sektör
    mov bx, 0x0000
    mov cl, 0x02
    mov al, 8
    call read_kernel_chunk
    mov al, '1'
    call debug_char

    ; Chunk 2: fiziksel 0x11000, sektör 10, 8 sektör
    mov bx, 0x1000
    mov cl, 0x0a
    mov al, 8
    call read_kernel_chunk
    mov al, '2'
    call debug_char

    ; Chunk 3: fiziksel 0x12000, sektör 18, 8 sektör
    mov bx, 0x2000
    mov cl, 0x12
    mov al, 8
    call read_kernel_chunk
    mov al, '3'
    call debug_char

    ; Chunk 4: fiziksel 0x13000, sektör 26, 8 sektör
    mov bx, 0x3000
    mov cl, 0x1a
    mov al, 8
    call read_kernel_chunk
    mov al, '4'
    call debug_char

    ; Chunk 5: fiziksel 0x14000, sektör 34, 8 sektör
    mov bx, 0x4000
    mov cl, 0x22
    mov al, 8
    call read_kernel_chunk
    mov al, '5'
    call debug_char

    ; Chunk 6: fiziksel 0x15000, sektör 42, 8 sektör
    mov bx, 0x5000
    mov cl, 0x2a
    mov al, 8
    call read_kernel_chunk
    mov al, '6'
    call debug_char

    ; Chunk 7: fiziksel 0x16000, sektör 50, 8 sektör
    mov bx, 0x6000
    mov cl, 0x32
    mov al, 8
    call read_kernel_chunk
    mov al, '7'
    call debug_char

    ; Chunk 8: fiziksel 0x17000, sektör 58, 6 sektör
    mov bx, 0x7000
    mov cl, 0x3a
    mov al, 6
    call read_kernel_chunk
    mov al, '8'
    call debug_char

    ; ES'i sıfırla
    xor ax, ax
    mov es, ax

    ret

read_kernel_chunk:
    mov ah, 0x02
    mov ch, 0x00
    mov dh, 0x00
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    ret

disk_error:
    mov al, 'E'           ; Hata durumunda ekranda 'BE' göreceksin
    call debug_char
    jmp $

debug_char:
    push ax
    push bx
    push dx
    mov dx, 0x00e9
    out dx, al
    mov ah, 0x0e
    xor bh, bh
    mov bl, 0x07
    int 0x10
    pop dx
    pop bx
    pop ax
    ret

set_vesa_mode:
    xor ax, ax
    mov es, ax
    mov ax, 0x4f01
    mov cx, VBE_MODE
    mov di, VBE_MODE_INFO
    int 0x10
    cmp ax, 0x004f
    jne vesa_error

    mov ax, 0x4f02
    mov bx, VBE_MODE | 0x4000
    int 0x10
    cmp ax, 0x004f
    jne vesa_error

    mov ebx, VBE_MODE_INFO
    ret

vesa_error:
    ; VBE yoksa eski VGA 13h'e düş ve kernel'e NULL bilgi ver.
    mov ax, 0x0013
    int 0x10
    xor ebx, ebx
    ret

[bits 16]
switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x7C000       ; Stack için güvenli alan
    mov esp, ebp
    call BEGIN_PM

[bits 32]
BEGIN_PM:
    call KERNEL_OFFSET      ; 0x10000 - kernel'in fiziksel adresi
    jmp $

; --- GDT (Global Descriptor Table) ---
gdt_start:
gdt_null: 
    dd 0x0, 0x0
gdt_code: 
    dw 0xffff, 0x0
    db 0x0, 10011010b, 11001111b, 0x0
gdt_data: 
    dw 0xffff, 0x0
    db 0x0, 10010010b, 11001111b, 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

BOOT_DRIVE db 0
times 510-($-$$) db 0
dw 0xaa55
