/*
 * RISC-V translation routines for the experimental Zispi extension.
 *
 * Copyright (c) 2026 Bytedance Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

static bool gen_zispi_store(DisasContext *ctx, arg_r *a, MemOp memop,
                            target_long increment)
{
    TCGv base;
    TCGv data;

    if (!ctx->cfg_ptr->ext_zispi || a->rs1 == 0 || a->rd != a->rs1) {
        return false;
    }

    base = get_gpr(ctx, a->rs1, EXT_NONE);
    data = get_gpr(ctx, a->rs2, EXT_NONE);
    memop |= ctx->mo_endianness;
    if (ctx->cfg_ptr->ext_zama16b) {
        memop |= MO_ATOM_WITHIN16;
    }
    decode_save_opc(ctx, 0);
    if (ctx->ztso) {
        tcg_gen_mb(TCG_MO_ALL | TCG_BAR_STRL);
    }

    /* A synchronous store exception occurs before the base writeback. */
    tcg_gen_qemu_st_tl(data, base, ctx->mem_idx, memop);
    tcg_gen_addi_tl(base, base, increment);
    gen_set_gpr(ctx, a->rs1, base);
    return true;
}

static bool trans_spi_b(DisasContext *ctx, arg_spi_b *a)
{
    return gen_zispi_store(ctx, a, MO_SB, 1);
}

static bool trans_spi_h(DisasContext *ctx, arg_spi_h *a)
{
    return gen_zispi_store(ctx, a, MO_SW, 2);
}

static bool trans_spi_w(DisasContext *ctx, arg_spi_w *a)
{
    return gen_zispi_store(ctx, a, MO_SL, 4);
}

static bool trans_spi_d(DisasContext *ctx, arg_spi_d *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_zispi_store(ctx, a, MO_SQ, 8);
}
