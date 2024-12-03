[org 0x7C00]           ; Starting address
[bits 16]              ; Real mode

section .code

; Set up video mode using VBE
.switch:
    mov ax, 0x4F01     ; This retrieves info about VESA modes         
    mov cx, 0x117      ; VBE mode to use: Try changing this and see what happens! kOSeki works with 16-bit color modes like: 111h, 114h, 117h
    mov bx, 0x0800     ; Buffer location     
    mov es, bx
    mov di, 0x00
    int 0x10           ; BIOS interrupt

    mov ax, 0x4F02     ; Set mode
    mov bx, 0x117      ; Also change this to switch VBE mode       
    int 0x10           


    ; Here we prepare for 32-bit (protected) mode
    xor ax, ax         ; Zeroes register
    mov ds, ax         ; Set DS and ES to zero
    mov es, ax

    mov bx, 0x9000     ; Address for kernel load sector  
    mov ah, 0x02       ; Read sectors
    mov al, 62         ; SECTOR COUNT [Change carefully]
    mov ch, 0x00       ; Cyl no.
    mov dh, 0x00       ; Head no.
    mov cl, 0x02       ; Sector no.
    int 0x13           ; BIOS interrupt


    cli                ; Clear all interrupts while loading GDT
    lgdt [gdt_descriptor]

    mov eax, cr0       ; Flipping the last bit of CR0 to enable 32-bit (protected) mode
    or eax, 0x1
    mov cr0, eax

    jmp code_seg:protected_start    ; Far jump

[bits 32]
protected_start:
    mov ax, data_seg   ; Load data segment selector
    mov ds, ax         ; Setting up registers
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    call 0x9000                 
    jmp $              ; Infinite loop              

gdt_begin:
gdt_null_descriptor:
    dd 0x00            ; Null descriptor
    dd 0x00

gdt_code_seg:
    dw 0xeeee          ; Code_seg limit            
    dw 0x0000          ; 16 bits   
    db 0x00            ; 8 bits
    db 10011010b              
    db 11001111b              
    db 0x00            ; 8 bits

gdt_data_seg:
    dw 0xeeee                 
    dw 0x0000                 
    db 0x00                   
    db 10010010b              
    db 11001111b              
    db 0x00                   

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_begin - 1  ; Size of GDT (bytes)
    dd gdt_begin                ; Address of GDT

code_seg equ gdt_code_seg - gdt_begin
data_seg equ gdt_data_seg - gdt_begin

times 510 - ($ - $$) db 0       ; This is padding, so basically it fills the remaining space with zeroes
dw 0xAA55              ; Boot sector signature
