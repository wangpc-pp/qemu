/*
 * RISC-V translation routines for the experimental Zibs extension, v0.1.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

static bool gen_bbsi(DisasContext *ctx, arg_bsi *a, TCGCond cond)
{
    TCGLabel *taken;
    TCGv bit;

    if (!ctx->cfg_ptr->ext_zibs || get_xl(ctx) == MXL_RV128 ||
        (get_xl(ctx) == MXL_RV32 && a->shamt >= 32)) {
        return false;
    }

    taken = gen_new_label();
    bit = tcg_temp_new();
    tcg_gen_extract_tl(bit, get_gpr(ctx, a->rs1, EXT_NONE), a->shamt, 1);
    tcg_gen_brcondi_tl(cond, bit, 0, taken);
    return gen_branch_target(ctx, taken, a->imm);
}

static bool trans_bbclri(DisasContext *ctx, arg_bbclri *a)
{
    return gen_bbsi(ctx, a, TCG_COND_EQ);
}

static bool trans_bbseti(DisasContext *ctx, arg_bbseti *a)
{
    return gen_bbsi(ctx, a, TCG_COND_NE);
}
