MEMINFO     equ 1<<0
BOOTDEVICE  equ 1<<1
CMDLINE     equ 1<<2
MODULECOUNT equ 1<<3
SYMT        equ 48 ; bits 4,5
MEMMAP      equ 1<<6
DRIVE       equ 1<<7
CONFIGT     equ 1<<8
BOOTLDNAME  equ 1<<9
APMT        equ 1<<10
VIDEO       equ 1<<11
VIDEO_FRAMEBUF equ 1<<12
FLAGS       equ MEMINFO | BOOTDEVICE 
PRISM_MAGIC   equ 0x1BADB002
CHECKSUM    equ -(PRISM_MAGIC + FLAGS)

BOOT_MAGIC equ 0x2BADB002

section .multiboot
    align 4
    dd PRISM_MAGIC
    dd FLAGS
    dd CHECKSUM

section .data
    align 4096

section .initial_stack, nobits
    align 4

stack_bottom:
    resb 104856 ;1MB
stack_top:

section .text
    global _start
    global PRISM_MAGIC
    global BOOT_MAGIC
    extern __kernel_bss_section_start
    extern __kernel_bss_section_end

; see linker.ld
_start:
    mov esp, stack_top
    
    mov edi, __kernel_bss_section_start
    mov ecx, __kernel_bss_section_end
    sub ecx, edi ; ecx = bss size
    xor eax, eax
    cld
    rep stosb ; pad
    
    extern kmain
    mov eax, BOOT_MAGIC
    push ebx
    push eax
    call kmain
loop:
    hlt
    jmp loop