section .text
    extern exception_handler
    extern irq_handler

interrupt_handler:
    pusha
    xor eax, eax
    mov ax, ds ; lower 16 bits of eax=ds
    push eax

    mov ax, 0x10 ; load segement descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    
    mov eax, [esp + 40]
    
    cmp eax, 32
    jl .call_exception
    cmp eax, 48
    jl .call_irq

.call_irq:
    call irq_handler
    jmp .done

.call_exception:
    call exception_handler

.done:
    add esp, 4 ; Clean up pushed ESP

    pop ebx ; reload og seg descriptor
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    popa
    add esp, 8 ; Cleanup err code and isr number
    
    iret

%macro EXCEPTION_NOERR 1
  global exception_%1
  exception_%1:
    cli
    push byte 0
    push byte %1
    jmp interrupt_handler
%endmacro

%macro EXCEPTION_ERR 1
  global exception_%1
  exception_%1:
    cli
    push byte %1
    jmp interrupt_handler
%endmacro

%macro IRQ 2
  global irq_%1
  irq_%1:
    cli
    push byte 0
    push byte %2
    jmp interrupt_handler
%endmacro

;handlers

EXCEPTION_NOERR 0
EXCEPTION_NOERR 1
EXCEPTION_NOERR 2
EXCEPTION_NOERR 3
EXCEPTION_NOERR 4
EXCEPTION_NOERR 5
EXCEPTION_NOERR 6
EXCEPTION_NOERR 7
EXCEPTION_ERR 8
EXCEPTION_NOERR 9
EXCEPTION_ERR 10
EXCEPTION_ERR 11
EXCEPTION_ERR 12
EXCEPTION_ERR 13
EXCEPTION_ERR 14
EXCEPTION_NOERR 15
EXCEPTION_NOERR 16
EXCEPTION_ERR 17
EXCEPTION_NOERR 18
EXCEPTION_NOERR 19
EXCEPTION_NOERR 20
EXCEPTION_NOERR 21
EXCEPTION_NOERR 22
EXCEPTION_NOERR 23
EXCEPTION_NOERR 24
EXCEPTION_NOERR 25
EXCEPTION_NOERR 26
EXCEPTION_NOERR 27
EXCEPTION_NOERR 28
EXCEPTION_NOERR 29
EXCEPTION_ERR 30
EXCEPTION_NOERR 31
EXCEPTION_NOERR 128

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47