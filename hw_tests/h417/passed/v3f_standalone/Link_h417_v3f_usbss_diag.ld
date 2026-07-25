ENTRY(_start)

__stack_size = 2048;
PROVIDE(_stack_size = __stack_size);

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 64K
    RAM_CODE (xrw) : ORIGIN = 0x20100000, LENGTH = 64K
    RAM_LOAD (xrw) : ORIGIN = 0x20110000, LENGTH = 512
    RAM_LOW (xrw) : ORIGIN = 0x20110200, LENGTH = 0x7E00
    RAM_USBSS (xrw) : ORIGIN = 0x20118000, LENGTH = 0x60000
    RAM_SHARED (xrw) : ORIGIN = 0x20178000, LENGTH = 32K
}

SECTIONS
{
    .init :
    {
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.init)))
        . = ALIGN(4);
    } >FLASH AT>FLASH

    .loadcodelalign :
    {
        . = ALIGN(4);
        PROVIDE(_loadcode_lma = .);
    } >FLASH AT>FLASH

    .loadcode :
    {
        . = ALIGN(4);
        PROVIDE(_loadcode_vma_start = .);
        *(.load)
        . = ALIGN(4);
        PROVIDE(_loadcode_vma_end = .);
    } >RAM_LOAD AT>FLASH

    .highcodelalign :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_lma = .);
    } >FLASH AT>FLASH

    .highcode :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_vma_start = .);
        *(.vector)
        KEEP(*(SORT_NONE(.vector_handler)))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.sdata2.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
        PROVIDE(_highcode_vma_end = .);
    } >RAM_CODE AT>FLASH

    .text :
    {
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.handle_reset)))
        . = ALIGN(4);
    } >FLASH AT>FLASH

    .shared (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(SORT_NONE(.shared_data)))
        . = ALIGN(4);
    } >RAM_SHARED

    .highcodelalign1 :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_lma1 = .);
    } >FLASH AT>FLASH

    .fini :
    {
        . = ALIGN(4);
        PROVIDE(_highcode_vma_start1 = .);
        KEEP(*(SORT_NONE(.fini)))
        . = ALIGN(4);
    } >RAM_CODE AT>FLASH

    PROVIDE(_etext = .);
    PROVIDE(_eitcm = .);

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >RAM_CODE AT>FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o) .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >RAM_CODE AT>FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o) .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >RAM_CODE AT>FLASH

    .ctors :
    {
        KEEP(*crtbegin.o(.ctors))
        KEEP(*crtbegin?.o(.ctors))
        KEEP(*(EXCLUDE_FILE (*crtend.o *crtend?.o) .ctors))
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
    } >RAM_CODE AT>FLASH

    .dtors :
    {
        KEEP(*crtbegin.o(.dtors))
        KEEP(*crtbegin?.o(.dtors))
        KEEP(*(EXCLUDE_FILE (*crtend.o *crtend?.o) .dtors))
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        . = ALIGN(4);
        PROVIDE(_highcode_vma_end1 = .);
    } >RAM_CODE AT>FLASH

    .dalign :
    {
        . = ALIGN(4);
        PROVIDE(_data_vma = .);
    } >RAM_LOW AT>FLASH

    .dlalign :
    {
        . = ALIGN(4);
        PROVIDE(_data_lma = .);
    } >FLASH AT>FLASH

    .data :
    {
        . = ALIGN(4);
        PROVIDE(_sdata = .);
        *(.gnu.linkonce.r.*)
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata)
        *(.sdata.*)
        *(.gnu.linkonce.s.*)
        . = ALIGN(8);
        *(.srodata.cst16)
        *(.srodata.cst8)
        *(.srodata.cst4)
        *(.srodata.cst2)
        *(.srodata)
        *(.srodata.*)
        . = ALIGN(4);
        PROVIDE(_edata = .);
    } >RAM_LOW AT>FLASH

    /* Match the proven product USBHS and USBFS buffer addresses exactly. */
    .usbhs_state 0x2011148C (NOLOAD) :
    {
        . = ALIGN(4);
        PROVIDE(_sbss = .);
        KEEP(*ch32h417_usbhs_hid_nkro.o(.bss.s_diag))
        . = ALIGN(4);
    } >RAM_LOW

    .usbhs_dma 0x201114E0 (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.bss.s_pending_report))
        KEEP(*(.bss.HID_Report_Buffer))
        KEEP(*(.bss.Data_Buffer))
        KEEP(*(.bss.RingBuffer_Comm))
        KEEP(*(.bss.USBHS_EP2_Tx_Buf))
        KEEP(*(.bss.USBHS_EP1_Rx_Buf))
        KEEP(*(.bss.USBHS_EP0_Buf))
        . = ALIGN(4);
    } >RAM_LOW

    .usbfs_state 0x20113B5C (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*ch32h417_usbfs_hid_nkro.o(.bss.s_diag))
        KEEP(*(.bss.USBFS_HID_Report_Buffer))
        . = ALIGN(4);
    } >RAM_LOW

    .usbfs_dma 0x20113BF0 (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.bss.USBFS_Data_Buffer))
        KEEP(*(.bss.USBFS_RingBuffer_Comm))
        KEEP(*(.bss.USBFS_EP4_Buf))
        KEEP(*(.bss.USBFS_EP3_Buf))
        KEEP(*(.bss.USBFS_EP2_Buf))
        KEEP(*(.bss.USBFS_EP0_Buf))
        . = ALIGN(4);
    } >RAM_LOW

    /* USBSS needs 98 KiB of endpoint buffers; keep them out of product V3 RAM. */
    .usbss_dma (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.bss.USBSS_EP3_Rx_Buf))
        KEEP(*(.bss.USBSS_EP2_Rx_Buf))
        KEEP(*(.bss.USBSS_EP1_Rx_Buf))
        KEEP(*(.bss.USBSS_EP0_Buf))
        . = ALIGN(4);
    } >RAM_USBSS

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        *(.sbss*)
        *(.gnu.linkonce.sb.*)
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON*)
        . = ALIGN(4);
        PROVIDE(_ebss = .);
    } >RAM_LOW

    PROVIDE(_end = _ebss);
    PROVIDE(end = .);

    .stack ORIGIN(RAM_LOW) + LENGTH(RAM_LOW) - __stack_size :
    {
        PROVIDE(_heap_end = .);
        . = ALIGN(4);
        PROVIDE(_susrstack = .);
        . = . + __stack_size;
        PROVIDE(_eusrstack = .);
        __freertos_irq_stack_top = .;
    } >RAM_LOW

    ASSERT(_edata <= 0x2011148C, "initialized data overlaps product USB buffers")
    ASSERT(ADDR(.usbhs_state) == 0x2011148C, "USBHS state base changed")
    ASSERT(SIZEOF(.usbhs_state) == 0x54, "USBHS state layout changed")
    ASSERT(ADDR(.usbhs_dma) == 0x201114E0, "USBHS DMA base changed")
    ASSERT(SIZEOF(.usbhs_dma) == 0x267C, "USBHS DMA layout changed")
    ASSERT(ADDR(.usbfs_state) == 0x20113B5C, "USBFS state base changed")
    ASSERT(SIZEOF(.usbfs_state) == 0x94, "USBFS state layout changed")
    ASSERT(ADDR(.usbfs_dma) == 0x20113BF0, "USBFS DMA base changed")
    ASSERT(SIZEOF(.usbfs_dma) == 0x514, "USBFS DMA layout changed")
    ASSERT(_ebss <= (ORIGIN(RAM_LOW) + LENGTH(RAM_LOW) - __stack_size),
           "V3 low RAM overlaps stack")
    ASSERT((ADDR(.usbss_dma) + SIZEOF(.usbss_dma)) <= ORIGIN(RAM_SHARED),
           "USBSS DMA overlaps shared RAM")
}
