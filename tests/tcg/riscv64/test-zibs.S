/*
 * Test the experimental Zibs v0.1 single-bit conditional branches.
 * Use raw encodings so the assembler need not support Zibs.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

    .option norvc
    .option norelax

    .macro bbsi set, rs1, bit, offset
    .word 0x67 | (\set << 12) | (1 << 14) | ((\bit & 32) << 8) | \
          (\rs1 << 15) | ((\bit & 31) << 20) | \
          ((\offset & 0x1000) << 19) | ((\offset & 0x7e0) << 20) | \
          ((\offset & 0x1e) << 7) | ((\offset & 0x800) >> 4)
    .endm

    .macro check_branch set, rs1, bit, taken
    bbsi \set, \rs1, \bit, (.Ltaken\@ - .)
    .if \taken
    j fail
    .else
    j .Ldone\@
    .endif
.Ltaken\@:
    .if !\taken
    j fail
    .endif
.Ldone\@:
    addi s0, s0, 1
    .endm

    .macro check_bit bit
    li t0, 0
    check_branch 0, 5, \bit, 1
    check_branch 1, 5, \bit, 0
    li t0, (1 << \bit)
    mv t1, t0
    check_branch 0, 5, \bit, 0
    check_branch 1, 5, \bit, 1
    bne t0, t1, fail
    li t0, ~(1 << \bit)
    mv t1, t0
    check_branch 0, 5, \bit, 1
    check_branch 1, 5, \bit, 0
    bne t0, t1, fail
    li t0, -1
    check_branch 0, 5, \bit, 0
    check_branch 1, 5, \bit, 1
    check_branch 0, 0, \bit, 1
    check_branch 1, 0, \bit, 0
    .endm

    .macro check_illegal set, bit, rs1=5
    lla s1, .Linsn\@
    li s2, 2
    lw s3, 0(s1)
    lla s4, .Lresume\@
.Linsn\@:
    bbsi \set, \rs1, \bit, 0
    j fail
.Lresume\@:
    addi s0, s0, 1
    .endm

    .macro check_forward set, value
    li t0, \value
.Linsn\@:
    bbsi \set, 5, 0, (.Ltarget\@ - .)
    j fail
#ifdef ZIBS_COMPRESSED
    .space 4094 - (. - .Linsn\@)
#else
    .space 4092 - (. - .Linsn\@)
#endif
.Ltarget\@:
    addi s0, s0, 1
    .balign 4
    .endm

    .macro check_backward set, value
    li t0, \value
    j .Linsn\@
.Ltarget\@:
    j .Ldone\@
    .space 4096 - (. - .Ltarget\@)
.Linsn\@:
    bbsi \set, 5, 0, (.Ltarget\@ - .)
    j fail
.Ldone\@:
    addi s0, s0, 1
    .endm

    .macro check_alignment set, value
    li t0, \value
#ifdef ZIBS_COMPRESSED
    bbsi \set, 5, 0, (.Ltarget\@ - .)
    .2byte 0
.Ltarget\@:
    addi s0, s0, 1
    .balign 4
#else
    lla s1, .Linsn\@
    li s2, 0
    addi s3, s1, 2
    lla s4, .Lresume\@
.Linsn\@:
    bbsi \set, 5, 0, 2
    j fail
.Lresume\@:
    addi s0, s0, 1
#endif
    .endm

    .text
    .global _start
_start:
    li s0, 1
    li s1, 0
    lla t0, trap_handler
    csrw mtvec, t0
    csrw medeleg, zero

#ifdef ZIBS_DISABLED
    /* Both opcodes must trap, including non-taken branches and x0. */
    .set bit_index, 0
    .rept 64
    li t0, 0
    check_illegal 0, bit_index
    check_illegal 1, bit_index
    li t0, -1
    check_illegal 0, bit_index
    check_illegal 1, bit_index
    .set bit_index, bit_index + 1
    .endr
    check_illegal 0, 0, 0
    check_illegal 1, 0, 0
#else
    .set bit_index, 0
    .rept __riscv_xlen
    check_bit bit_index
    .set bit_index, bit_index + 1
    .endr

    /* Check zero offsets when not taken. */
    li t0, 1
    bbsi 0, 5, 0, 0
    li t0, 0
    bbsi 1, 5, 0, 0

    /* Exercise both signs and the endpoints of the B-immediate. */
    check_forward 0, 0
    check_forward 1, 1
    check_backward 0, 0
    check_backward 1, 1

    /* A misaligned target must not fault when the branch is not taken. */
    li t0, 1
    bbsi 0, 5, 0, 2
    li t0, 0
    bbsi 1, 5, 0, 2
    check_alignment 0, 0
    check_alignment 1, 1

#if __riscv_xlen == 32
    /* shamt[5] is reserved on RV32, regardless of the branch condition. */
    .set bit_index, 32
    .rept 32
    check_illegal 0, bit_index
    check_illegal 1, bit_index
    .set bit_index, bit_index + 1
    .endr
#endif
#endif

    li a0, 0
    j _exit

trap_handler:
    beqz s1, fail
    csrr t1, mepc
    bne t1, s1, fail
    csrr t1, mcause
    bne t1, s2, fail
    csrr t1, mtval
    bne t1, s3, fail
    csrw mepc, s4
    li s1, 0
    mret

fail:
    /* Keep failures nonzero after the host truncates the exit status. */
    ori a0, s0, 1
_exit:
    lla a1, semiargs
    li t0, 0x20026
#if __riscv_xlen == 32
    sw t0, 0(a1)
    sw a0, 4(a1)
#else
    sd t0, 0(a1)
    sd a0, 8(a1)
#endif
    li a0, 0x20
    .balign 16
    slli zero, zero, 0x1f
    ebreak
    srai zero, zero, 0x7
    j .

    .data
    .balign 8
semiargs:
    .space 16
