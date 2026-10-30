/*
 * RISC-V Zimemops bulk memory operations, version 0.1.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

static bool trans_zimemops(DisasContext *ctx, arg_r *a, bool fill)
{
    if (!ctx->cfg_ptr->ext_zimemops || get_xl(ctx) == MXL_RV128 ||
        a->rd == 0 || a->rs2 == 0 || (!fill && a->rs1 == 0) ||
        a->rd == a->rs1 || a->rd == a->rs2 || a->rs1 == a->rs2) {
        return false;
    }

    decode_save_opc(ctx, 0);
    gen_helper_zimemops(tcg_env, tcg_constant_i32(ctx->opcode));
    return true;
}

static bool trans_mcopy(DisasContext *ctx, arg_mcopy *a)
{
    return trans_zimemops(ctx, a, false);
}

static bool trans_mmove(DisasContext *ctx, arg_mmove *a)
{
    return trans_zimemops(ctx, a, false);
}

static bool trans_mset(DisasContext *ctx, arg_mset *a)
{
    return trans_zimemops(ctx, a, true);
}
