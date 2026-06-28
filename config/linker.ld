ENTRY(_start)

SECTIONS
{
    __kernel_section_start = .;
    
    .text 0x0100000 : {
        __kernel_text_section_start = .;
        *(.multiboot)
        *(.text)
        . = ALIGN(4096);
        __kernel_rodata_section_start = .;
        *(.rodata*)
        . = ALIGN(4096);
        __kernel_rodata_section_end = .;
        __kernel_text_section_end = .;
    }

    /* init section */
    .data : {
        __kernel_data_section_start = .;
        data = .; _data = .; __data = .;
        *(.data)
        . = ALIGN(4096);
        __kernel_data_section_end = .;
    }

    /* un-init section */
    .bss : {
        __kernel_bss_section_start = .;
        bss = .; _bss = .; __bss = .;
        *(.bss)
        . = ALIGN(4096);
        __kernel_bss_section_end = .;
    }

    /DISCARD/ : {
        *(.note.GNU-stack)
    }
    end = .; _end = .; __end = .;
    __kernel_section_end = .;
}