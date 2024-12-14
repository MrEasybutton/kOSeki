[bits 32]           ; In protected mode

START:
    ; External entry point (this is in main.c)

    extern start    
    call start      
    jmp $           ; Infinite loop


extern _idt, HandleISR1, HandleISR12
global isr1, isr12
global LoadIDT

IDTDesc:
    dw 2048         ; IDT size (bytes)
    dd _idt         ; Base address of the IDT


isr1:
    pusha           
    call HandleISR1 ; Call handler ISR1
    popa            ; Restore registers
    iret            ; Return from interrupt


isr12:
    pusha                  
    call HandleISR12     
    popa                  
    iret                  


LoadIDT:
    lidt [IDTDesc]  ; Load address and IDT size
    sti             ; Enable hardware interrupts
    ret             


