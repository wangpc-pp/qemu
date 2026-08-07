    .section .text
    .globl _start
_start:
    la      t2, query8
    li      t0, 5
    vsetvli zero, t0, e8, m1, ta, ma
    vle8.v  v8, (t2)
    li      a6, 0x44332211
    .word   0xd28840d7
    la      t2, result
    vsm.v   v1, (t2)
    lbu     t3, 0(t2)
    andi    t3, t3, 0x1f
    li      t4, 0x07
    bne     t3, t4, fail

    la      t2, query16
    li      t0, 4
    vsetvli zero, t0, e16, m1, ta, ma
    vle16.v v8, (t2)
    li      a6, 0x22221111
    .word   0xd28840d7
    la      t2, result
    vsm.v   v1, (t2)
    lbu     t3, 0(t2)
    andi    t3, t3, 0x0f
    li      t4, 0x03
    bne     t3, t4, fail

    la      t2, query8
    li      t0, 5
    vsetvli zero, t0, e8, m1, ta, ma
    vle8.v  v8, (t2)
    .word   0xd28040d7
    la      t2, result
    vsm.v   v1, (t2)
    lbu     t3, 0(t2)
    andi    t3, t3, 0x1f
    li      t4, 0x10
    bne     t3, t4, fail

    li      a0, 0
    li      a7, 93
    ecall

fail:
    li      a0, 1
    li      a7, 93
    ecall

    .section .rodata
    .balign 16
query8:
    .byte   0x11, 0x44, 0x22, 0x55, 0x00

    .balign 16
query16:
    .half   0x1111, 0x2222, 0x3333, 0x4444

    .section .bss
    .balign 16
result:
    .space  16
