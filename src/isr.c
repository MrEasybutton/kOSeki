#include "isr.h"
#include "idt.h"
#include "serial.h"
#include "8259_pic.h"
#include "utils.h"
#include "console.h"

#define MAX_HANDLERS_PER_INT 4

static ISR g_interrupt_handlers[NO_INTERRUPT_HANDLERS][MAX_HANDLERS_PER_INT];

static const char *exception_messages[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "BOUND Range Exceeded", "Invalid Opcode", "Device Not Available (No Math Coprocessor)",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection", "Page Fault", "Unknown Interrupt (intel reserved)",
    "x87 FPU Floating-Point Error (Math Fault)", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

void register_interrupt_handler(int num, ISR handler) {
    if (num >= NO_INTERRUPT_HANDLERS) return;
    
    for (int i = 0; i < MAX_HANDLERS_PER_INT; i++) {
        if (g_interrupt_handlers[num][i] == NULL) {
            g_interrupt_handlers[num][i] = handler;
            printf("IRQ %d registered at slot %d\n", num, i);
            return;
        }
    }
    panic("too many handlers for one interrupt.");
}

void end_interrupt(int num) {
    pic8259_eoi(num);
}

void irq_handler(REGISTERS *reg) {
    for (int i = 0; i < MAX_HANDLERS_PER_INT; i++) {
        if (g_interrupt_handlers[reg->int_no][i] != NULL) {
            g_interrupt_handlers[reg->int_no][i](reg);
        }
    }
    pic8259_eoi(reg->int_no);
}

static void print_registers(REGISTERS *reg) {
    printf("REGISTERS:\n");
    printf("err_code=%d\n", reg->err_code);
    printf("eax=0x%x, ebx=0x%x, ecx=0x%x, edx=0x%x\n", reg->eax, reg->ebx, reg->ecx, reg->edx);
    printf("edi=0x%x, esi=0x%x, ebp=0x%x, esp=0x%x\n", reg->edi, reg->esi, reg->ebp, reg->esp);
    printf("eip=0x%x, cs=0x%x, ss=0x%x, eflags=0x%x, useresp=0x%x\n", reg->eip, reg->cs, reg->ss, reg->eflags, reg->useresp);
}

void exception_handler(REGISTERS *reg) {
    if (reg->int_no < 32) {
        printf("EXCEPTION: %s\n", exception_messages[reg->int_no]);
        kprint("EXCEPTION: %s\n", exception_messages[reg->int_no]);
        print_registers(reg);
    }
    
    BOOL handled = FALSE;
    for (int i = 0; i < MAX_HANDLERS_PER_INT; i++) {
        if (g_interrupt_handlers[reg->int_no][i] != NULL) {
            g_interrupt_handlers[reg->int_no][i](reg);
            handled = TRUE;
        }
    }

    if (!handled && reg->int_no < 32) {
        panic("UNHANDLED EXCEPTION");
    }
}
