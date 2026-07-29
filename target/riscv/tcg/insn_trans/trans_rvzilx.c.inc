/*
 * RISC-V translation routines for the Zilx Standard Extension.
 *
 * Copyright (c) 2025 Bytedance Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define REQUIRE_ZILX(ctx) do {          \
    if (!ctx->cfg_ptr->ext_zilx) {      \
        return false;                   \
    }                                   \
} while (0)

/*
 * Zilx indexed loads compute their effective address from an index register
 * (rs1) and a base register (rs2):
 *
 *   EA = X[rs2] + (X[rs1] << scale)
 *
 * For the scaled unsigned-word-index forms (lxsuw*), the index is first
 * zero-extended from bits [31:0] before scaling.  Apart from the address
 * calculation, each instruction behaves exactly like the corresponding base
 * integer load of the same access size and signedness.
 */
static bool gen_indexed_load(DisasContext *ctx, arg_r *a, MemOp memop,
                             int scale, bool zext)
{
    REQUIRE_ZILX(ctx);

    TCGv dest = dest_gpr(ctx, a->rd);
    TCGv index = get_gpr(ctx, a->rs1, EXT_NONE);
    TCGv offs;
    TCGv addr;

    memop |= ctx->mo_endianness;
    if (ctx->cfg_ptr->ext_zama16b) {
        memop |= MO_ATOM_WITHIN16;
    }

    if (zext) {
        /* Zero-extend the low 32 bits of the index before scaling. */
        offs = tcg_temp_new();
        tcg_gen_ext_tl(offs, index, MO_UL);
        if (scale > 0) {
            tcg_gen_shli_tl(offs, offs, scale);
        }
    } else if (scale > 0) {
        offs = tcg_temp_new();
        tcg_gen_shli_tl(offs, index, scale);
    } else {
        offs = index;
    }

    addr = get_address_indexed(ctx, a->rs2, offs);

    decode_save_opc(ctx, 0);
    tcg_gen_qemu_ld_tl(dest, addr, ctx->mem_idx, memop);
    gen_set_gpr(ctx, a->rd, dest);

    if (ctx->ztso) {
        tcg_gen_mb(TCG_MO_ALL | TCG_BAR_LDAQ);
    }

    return true;
}

/* funct5=10010: unscaled indexed loads (lx*). */
static bool trans_lxh(DisasContext *ctx, arg_lxh *a)
{
    return gen_indexed_load(ctx, a, MO_SW, 0, false);
}

static bool trans_lxw(DisasContext *ctx, arg_lxw *a)
{
    return gen_indexed_load(ctx, a, MO_SL, 0, false);
}

static bool trans_lxd(DisasContext *ctx, arg_lxd *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SQ, 0, false);
}

static bool trans_lxhu(DisasContext *ctx, arg_lxhu *a)
{
    return gen_indexed_load(ctx, a, MO_UW, 0, false);
}

static bool trans_lxwu(DisasContext *ctx, arg_lxwu *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_UL, 0, false);
}

/* funct5=11010: scaled indexed loads (lxs*). */
static bool trans_lxsb(DisasContext *ctx, arg_lxsb *a)
{
    return gen_indexed_load(ctx, a, MO_SB, 0, false);
}

static bool trans_lxsh(DisasContext *ctx, arg_lxsh *a)
{
    return gen_indexed_load(ctx, a, MO_SW, 1, false);
}

static bool trans_lxsw(DisasContext *ctx, arg_lxsw *a)
{
    return gen_indexed_load(ctx, a, MO_SL, 2, false);
}

static bool trans_lxsd(DisasContext *ctx, arg_lxsd *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SQ, 3, false);
}

static bool trans_lxsbu(DisasContext *ctx, arg_lxsbu *a)
{
    return gen_indexed_load(ctx, a, MO_UB, 0, false);
}

static bool trans_lxshu(DisasContext *ctx, arg_lxshu *a)
{
    return gen_indexed_load(ctx, a, MO_UW, 1, false);
}

static bool trans_lxswu(DisasContext *ctx, arg_lxswu *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_UL, 2, false);
}

/*
 * funct5=11110: scaled loads with zero-extended 32-bit index (lxsuw*).
 * These forms are defined only for RV64.
 */
static bool trans_lxsuwb(DisasContext *ctx, arg_lxsuwb *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SB, 0, true);
}

static bool trans_lxsuwh(DisasContext *ctx, arg_lxsuwh *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SW, 1, true);
}

static bool trans_lxsuww(DisasContext *ctx, arg_lxsuww *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SL, 2, true);
}

static bool trans_lxsuwd(DisasContext *ctx, arg_lxsuwd *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_SQ, 3, true);
}

static bool trans_lxsuwbu(DisasContext *ctx, arg_lxsuwbu *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_UB, 0, true);
}

static bool trans_lxsuwhu(DisasContext *ctx, arg_lxsuwhu *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_UW, 1, true);
}

static bool trans_lxsuwwu(DisasContext *ctx, arg_lxsuwwu *a)
{
    REQUIRE_64_OR_128BIT(ctx);
    return gen_indexed_load(ctx, a, MO_UL, 2, true);
}
