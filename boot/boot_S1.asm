[org 0x7C00:0000]
[bits 16]

section .code

.switch:
    mov ax, 0x4F01 ; retrieve info bout VBE
    mov cx, 0x4118 ; the mode (we using 118h)
    mov bx, 0x0800
    mov es, bx
    mov di, 0x00
    int 0x10

    mov ax, 0x4F02
    mov bx, 0x4118 ; also change mode here
    int 0x10           

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov bx, 0x9000
    mov ah, 0x02
    mov al, 128 ; SECTORS (i reached the limit i think)
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    int 0x13
    jc .disk_error
    jmp .disk_success
    
.disk_error:
    xor ax, ax
    int 0x13
    
    mov bx, 0x9000
    mov ah, 0x02
    mov al, 255
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    int 0x13
    
.disk_success:
    xor ax, ax
    mov es, ax


    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp code_seg:protected_start

[bits 32]
protected_start:
    mov ax, data_seg
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    call 0x9000                 
    jmp $

gdt_begin:
gdt_null_descriptor:
    dd 0x00
    dd 0x00

gdt_code_seg:
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10011010b              
    db 11001111b              
    db 0x00

gdt_data_seg:
    dw 0xffff                 
    dw 0x0000                 
    db 0x00                   
    db 10010010b              
    db 11001111b              
    db 0x00                   

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_begin - 1
    dd gdt_begin

code_seg equ gdt_code_seg - gdt_begin
data_seg equ gdt_data_seg - gdt_begin

times 510 - ($ - $$) db 0
dw 0xAA55