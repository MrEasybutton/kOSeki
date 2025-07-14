[bits 32]

START:
    extern start    
    call start      
    jmp $


extern _idt, HandleISR1, HandleISR12
global isr1, isr12
global LoadIDT

IDTDesc:
    dw 2048
    dd _idt


isr1:
    pusha           
    call HandleISR1
    popa
    iret

isr12:
    pusha                  
    call HandleISR12     
    popa                  
    iret                  

LoadIDT:
    lidt [IDTDesc]
    sti
    ret