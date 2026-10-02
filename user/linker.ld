OUTPUT_FORMAT(elf64-x86-64)
ENTRY(user_main)
SECTIONS {
    . = 0x400000;
    .text : { *(.text.entry) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "user globals must be read-only")
    /DISCARD/ : { *(.eh_frame*) *(.note .note.*) }
}
