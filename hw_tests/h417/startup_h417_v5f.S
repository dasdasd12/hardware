    .section .init,"ax",@progbits
    .global _start
    .align 2

_start:
    j handle_reset

handle_reset:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, _eusrstack

    la a0, _data_lma
    la a1, _sdata
    la a2, _edata
    bgeu a1, a2, 2f
1:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b

2:
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, 4f
3:
    sw zero, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, 3b

4:
    call entry

5:
    j 5b
