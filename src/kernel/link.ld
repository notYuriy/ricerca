ENTRY(kernel_init)

SECTIONS {
    /* Base of kernel physical window + 2 GB */
    . = 0xffffffff80200000;

    /* Stivale 2 info for the bootloader */
    .stivale2hdr : ALIGN(4K) {
        KEEP(*(.stivale2hdr))
    }

    /* Kernel code */
    _kernel_text_start = .;
    .text : ALIGN(4K) {
        *(.text*)
    }
    _kernel_text_end = .;

    /* Readonly kernel data */
    _kernel_rodata_start = .;
    .rodata : ALIGN(4K) {
        *(.rodata*)
    }
    _kernel_rodata_end = .;

    /* Readable and writable kernel data */
    _kernel_data_start = .;
    .data : ALIGN(4K) {
        *(.data*)
    }
    _kernel_data_end = .;

    /* Readable writeable unitialized kernel memory */
    _kernel_bss_start = .;
    .bss : ALIGN(4K) {
        *(.bss*)
    }
    _kernel_bss_end = .;
}
