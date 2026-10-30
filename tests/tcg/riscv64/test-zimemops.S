/*
 * Hand-encoded Zimemops v0.1 functional and restart tests.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .option norvc
    .option norelax

    .macro memop op, rd=a0, rs1=a1, rs2=a2
    .insn r 0x0f, 3, \op, \rd, \rs1, \rs2
    .endm

    .macro check reg, value
    li      t6, \value
    bne     \reg, t6, fail
    .endm

    .macro invalid op, rd, rs1, rs2, count=0
    li      a0, -1
    li      a1, -2
    li      a2, \count
    mv      s0, a0
    mv      s1, a1
    mv      s2, a2
    li      s8, 1
    li      s9, 2
    la      s10, 991f
991:
    memop   \op, \rd, \rs1, \rs2
    bnez    s8, fail
    .endm

    .macro restrict_memory
    la      t0, denied
    srli    t0, t0, 2
    csrw    pmpaddr0, t0
    la      t0, denied_end
    srli    t0, t0, 2
    csrw    pmpaddr1, t0
    li      t0, -1
    csrw    pmpaddr2, t0
    li      t0, 0x0f080f /* TOR: RWX, no access, RWX. */
    csrw    pmpcfg0, t0
    li      t0, 3 << 11
    csrc    mstatus, t0
    li      t0, (1 << 17) | (1 << 11) /* MPRV, MPP=S. */
    csrs    mstatus, t0
    .endm

    .macro fault op, cause, address, mode=2
    li      s8, \mode
    li      s9, \cause
    la      s10, 992f
    la      s11, \address
    restrict_memory
992:
    memop   \op
    bnez    s8, fail
    .endm

    .text
    .globl _start
_start:
    la      t0, trap
    csrw    mtvec, t0
    li      s8, 0
    li      s6, 0
    li      gp, 1

#ifdef ZIMEMOPS_DISABLED
    .irp op, 0, 3, 6
    invalid \op, a0, a1, a2
    invalid \op, a0, a1, a2, 1
    .endr
    li      gp, 0
    j       exit
#endif

    /* Unaligned copy, exact byte bounds and final operand values. */
    la      a0, buffer
    addi    a0, a0, 1
    la      a1, source
    addi    a1, a1, 3
    li      a2, 37
    addi    s0, a0, 37
    addi    s1, a1, 37
    memop   0
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    la      t0, buffer
    lbu     t1, 0(t0)
    check   t1, 0x5a
    lbu     t1, 38(t0)
    check   t1, 0x5a
    addi    t0, t0, 1
    li      t1, 3
1:
    lbu     t2, 0(t0)
    bne     t1, t2, fail
    addi    t0, t0, 1
    addi    t1, t1, 1
    li      t2, 40
    bne     t1, t2, 1b

    /* Backward overlapping move keeps both low-address bases. */
    li      gp, 2
    la      a1, move_buffer
    addi    a0, a1, 3
    li      a2, 45
    mv      s0, a0
    mv      s1, a1
    memop   3
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    mv      t0, s0
    li      t1, 0
2:
    lbu     t2, 0(t0)
    bne     t1, t2, fail
    addi    t0, t0, 1
    addi    t1, t1, 1
    li      t2, 45
    bne     t1, t2, 2b

    /* Move the same bytes back in the forward direction. */
    li      gp, 3
    mv      a0, s1
    mv      a1, s0
    li      a2, 45
    addi    s0, a0, 45
    addi    s1, a1, 45
    memop   3
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    la      t0, move_buffer
    li      t1, 0
3:
    lbu     t2, 0(t0)
    bne     t1, t2, fail
    addi    t0, t0, 1
    addi    t1, t1, 1
    li      t2, 45
    bne     t1, t2, 3b

    /* Fill ignores high bits, preserves rs1, and supports x0. */
    li      gp, 4
    la      a0, buffer
    li      a1, 0x1234ab
    li      a2, 41
    addi    s0, a0, 41
    memop   6
    bne     a0, s0, fail
    check   a1, 0x1234ab
    bnez    a2, fail
    la      t0, buffer
4:
    lbu     t1, 0(t0)
    check   t1, 0xab
    addi    t0, t0, 1
    bne     t0, s0, 4b
    la      a0, buffer
    li      a2, 41
    memop   6, a0, zero, a2
    bne     a0, s0, fail
    bnez    a2, fail
    la      t0, buffer
5:
    lbu     t1, 0(t0)
    bnez    t1, fail
    addi    t0, t0, 1
    bne     t0, s0, 5b

    /* Zero length must not check even invalid operand addresses. */
    li      gp, 5
    .irp op, 0, 3, 6
    li      a0, -1
    li      a1, 1
    li      a2, 0
    memop   \op
    check   a0, -1
    check   a1, 1
    bnez    a2, fail
    .endr

    /* Reserved registers fault even with zero length. */
    li      gp, 6
    .irp n, 0, 1
    .irp op, 0, 3, 6
    invalid \op, zero, a1, a2, \n
    invalid \op, a0, a1, zero, \n
    invalid \op, a0, a0, a2, \n
    invalid \op, a0, a1, a0, \n
    invalid \op, a0, a1, a1, \n
    .endr
    invalid 0, a0, zero, a2, \n
    invalid 3, a0, zero, a2, \n
    .endr
    .irp op, 1, 2, 4, 5, 7, 127
    invalid \op, a0, a1, a2
    .endr

    /* A destination fault exposes only the three committed bytes. */
    li      gp, 7
    la      a0, denied
    addi    a0, a0, -3
    la      a1, source
    li      a2, 8
    addi    s0, a0, 3
    addi    s1, a1, 3
    li      s2, 5
    fault   0, 7, denied
    addi    s0, s0, 5
    addi    s1, s1, 5
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    la      t0, denied
    addi    t0, t0, -3
    la      t1, source
    li      t2, 8
    call    compare

    /* A source fault produces no store or progress for its byte. */
    li      gp, 8
    la      a0, buffer
    la      a1, denied
    addi    a1, a1, -3
    li      a2, 8
    addi    s0, a0, 3
    addi    s1, a1, 3
    li      s2, 5
    fault   0, 5, denied
    addi    s0, s0, 5
    addi    s1, s1, 5
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail

    /* Fill restarts with the unchanged fill register. */
    li      gp, 9
    la      a0, denied
    addi    a0, a0, -3
    li      a1, 0x5c
    li      a2, 8
    addi    s0, a0, 3
    mv      s1, a1
    li      s2, 5
    fault   6, 7, denied
    addi    s0, s0, 5
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail

    /* Forward mmove has the same restart cursors as mcopy. */
    li      gp, 10
    la      a0, buffer
    la      a1, denied
    addi    a1, a1, -3
    li      a2, 8
    addi    s0, a0, 3
    addi    s1, a1, 3
    li      s2, 5
    fault   3, 5, denied
    addi    s0, s0, 5
    addi    s1, s1, 5
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail

    /* Backward source fault: eight tail bytes, unchanged bases. */
    li      gp, 11
    la      a0, high_buffer
    la      a1, denied
    addi    a1, a1, -8
    li      a2, 32
    mv      s0, a0
    mv      s1, a1
    li      s2, 24
    fault   3, 5, denied_last
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    mv      t0, s0
    mv      t1, s1
    li      t2, 32
    call    compare

    /* Backward destination fault obeys the same suffix progress. */
    li      gp, 12
    la      a0, denied
    addi    a0, a0, -8
    la      a1, source
    li      a2, 32
    mv      s0, a0
    mv      s1, a1
    li      s2, 24
    fault   3, 7, denied_last
    bne     a0, s0, fail
    bne     a1, s1, fail
    bnez    a2, fail
    mv      t0, s0
    mv      t1, s1
    li      t2, 32
    call    compare

    /* The XLEN high bit denotes a positive unsigned byte count. */
    li      gp, 13
    la      a0, buffer
    la      a1, denied
    li      a2, 1
    slli    a2, a2, (__riscv_xlen - 1)
    mv      s0, a0
    mv      s1, a1
    mv      s2, a2
    fault   0, 5, denied, 1

    /* Device accesses fault before causing any MMIO side effect. */
    li      gp, 14
    la      a0, buffer
    li      a1, 0x10000000 /* UART */
    li      a2, 1
    mv      s0, a0
    mv      s1, a1
    mv      s2, a2
    li      s8, 1
    li      s9, 5
    la      s10, 6f
    mv      s11, a1
6:
    memop   0
    bnez    s8, fail
    li      gp, 15
    li      a0, 0x10000000
    li      a1, 0x55
    li      a2, 1
    mv      s0, a0
    mv      s1, a1
    mv      s2, a2
    li      s8, 1
    li      s9, 7
    la      s10, 7f
    mv      s11, a0
7:
    memop   6
    bnez    s8, fail

    /* Equal addresses still read and write, including write faults. */
    li      gp, 16
    la      a0, denied
    mv      a1, a0
    li      a2, 1
    mv      s0, a0
    mv      s1, a1
    mv      s2, a2
    li      t0, 0x0f090f /* Middle TOR entry is read-only. */
    csrw    pmpcfg0, t0
    li      s8, 1
    li      s9, 7
    la      s10, 8f
    mv      s11, a0
    li      t0, 3 << 11
    csrc    mstatus, t0
    li      t0, (1 << 17) | (1 << 11)
    csrs    mstatus, t0
8:
    memop   3
    bnez    s8, fail

    /* Normal and zero-length operations each retire once. */
#ifdef CHECK_INSTRET
    li      gp, 17
    .irp op, 0, 3, 6
    la      a0, buffer
    la      a1, source
    li      a2, 32
    csrr    s0, minstret
    memop   \op
    csrr    s1, minstret
    sub     s1, s1, s0
    check   s1, 2
    li      a2, 0
    csrr    s0, minstret
    memop   \op
    csrr    s1, minstret
    sub     s1, s1, s0
    check   s1, 2
    .endr
#endif

#ifdef CHECK_INSTRET
    /* Partial work before a data fault does not retire the instruction. */
    li      gp, 18
    la      a0, denied
    addi    a0, a0, -3
    la      a1, source
    li      a2, 8
    addi    s0, a0, 3
    addi    s1, a1, 3
    li      s2, 5
    li      s8, 1
    li      s9, 7
    la      s10, 12f
    la      s11, denied
    li      s6, 1
    restrict_memory
    csrr    s4, minstret
12:
    memop   0
    bnez    s8, fail
    bnez    s6, fail
#endif
    li      gp, 0
    j       exit

compare:
    lbu     t3, 0(t0)
    lbu     t4, 0(t1)
    bne     t3, t4, fail
    addi    t0, t0, 1
    addi    t1, t1, 1
    addi    t2, t2, -1
    bnez    t2, compare
    ret

    .balign 4
trap:
#ifdef CHECK_INSTRET
    csrr    s5, minstret
#endif
    beqz    s8, fail
    csrr    t0, mcause
    bne     t0, s9, fail
    csrr    t0, mepc
    bne     t0, s10, fail
    bne     a0, s0, fail
    bne     a1, s1, fail
    bne     a2, s2, fail
#ifdef CHECK_INSTRET
    beqz    s6, 13f
    sub     t0, s5, s4
    check   t0, 1
    li      s6, 0
13:
#endif
    li      t0, 2
    beq     s9, t0, 9f
    csrr    t0, mtval
    bne     t0, s11, fail
9:
    li      t0, 1 << 17
    csrc    mstatus, t0
    li      t0, 0x0f0f0f /* Restore access to the denied region. */
    csrw    pmpcfg0, t0
    li      t0, 2
    beq     s8, t0, 10f /* Restart the same instruction. */
    addi    s10, s10, 4
    csrw    mepc, s10
10:
    li      s8, 0
    mret

fail:
    bnez    gp, exit
    li      gp, 255
exit:
    /* The virt machine's test finisher works at both XLENs. */
    li      t0, 0x100000
    beqz    gp, 11f
    slli    t1, gp, 16
    li      t2, 0x3333
    or      t1, t1, t2
    sw      t1, 0(t0)
11:
    li      t1, 0x5555
    sw      t1, 0(t0)
    j       .

    .data
    .balign 16
source:
    .set i, 0
    .rept 64
    .byte i
    .set i, i + 1
    .endr
buffer:
    .fill 128, 1, 0x5a
move_buffer:
    .set i, 0
    .rept 64
    .byte i
    .set i, i + 1
    .endr
    .balign 16
    .fill 16, 1, 0x6b
denied:
    .fill 15, 1, 0x6b
denied_last:
    .byte 0x6b
denied_end:
    .fill 32, 1, 0x6b
high_buffer:
    .fill 64, 1, 0x5a
