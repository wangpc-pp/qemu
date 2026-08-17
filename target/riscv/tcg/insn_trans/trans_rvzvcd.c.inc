/*
 * RISC-V translation routines for the Zvcd vector conflict detection extension
 *
 * Copyright (c) 2026
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

/*
 * Zvcd requires an integer vector environment (Zve32x) and only supports
 * SEW=32 and SEW=64.  Other SEWs are reserved.
 */
static bool vext_zvcd_base_check(DisasContext *s)
{
    return s->cfg_ptr->ext_zvcd &&
           (s->sew == MO_32 || s->sew == MO_64);
}

/*
 * vconflictcnt.v vd, vs2, vm
 *
 * The destination data group (EEW=SEW, EMUL=LMUL) must not overlap the source
 * data group.  vstart must be zero.
 */
static bool trans_vconflictcnt_v(DisasContext *s, arg_rmr *a)
{
    if (require_rvv(s) &&
        vext_zvcd_base_check(s) &&
        vext_check_isa_ill(s) &&
        require_align(a->rd, s->lmul) &&
        require_align(a->rs2, s->lmul) &&
        require_vm(a->vm, a->rd) &&
        require_noover(a->rd, s->lmul, a->rs2, s->lmul) &&
        s->vstart_eq_zero) {
        uint32_t data = 0;
        static gen_helper_gvec_3_ptr * const fns[2] = {
            gen_helper_vconflictcnt_v_w, gen_helper_vconflictcnt_v_d,
        };

        data = FIELD_DP32(data, VDATA, VM, a->vm);
        data = FIELD_DP32(data, VDATA, LMUL, s->lmul);
        data = FIELD_DP32(data, VDATA, VTA, s->vta);
        data = FIELD_DP32(data, VDATA, VMA, s->vma);
        tcg_gen_gvec_3_ptr(vreg_ofs(s, a->rd), vreg_ofs(s, 0),
                           vreg_ofs(s, a->rs2), tcg_env,
                           s->cfg_ptr->vlenb, s->cfg_ptr->vlenb,
                           data, fns[s->sew - MO_32]);
        finalize_rvv_inst(s);
        return true;
    }
    return false;
}

/*
 * vconflictlast.m vd, vs2, vm
 *
 * The destination is a mask register (EEW=1, single vector register) and must
 * not overlap the source data group.  vstart must be zero.
 */
static bool trans_vconflictlast_m(DisasContext *s, arg_rmr *a)
{
    if (require_rvv(s) &&
        vext_zvcd_base_check(s) &&
        vext_check_isa_ill(s) &&
        require_align(a->rs2, s->lmul) &&
        require_vm(a->vm, a->rd) &&
        !is_overlapped(a->rd, 1, a->rs2, 1 << MAX(s->lmul, 0)) &&
        s->vstart_eq_zero) {
        uint32_t data = 0;
        static gen_helper_gvec_3_ptr * const fns[2] = {
            gen_helper_vconflictlast_m_w, gen_helper_vconflictlast_m_d,
        };

        data = FIELD_DP32(data, VDATA, VM, a->vm);
        data = FIELD_DP32(data, VDATA, LMUL, s->lmul);
        data = FIELD_DP32(data, VDATA, VTA_ALL_1S, s->cfg_vta_all_1s);
        data = FIELD_DP32(data, VDATA, VMA, s->vma);
        tcg_gen_gvec_3_ptr(vreg_ofs(s, a->rd), vreg_ofs(s, 0),
                           vreg_ofs(s, a->rs2), tcg_env,
                           s->cfg_ptr->vlenb, s->cfg_ptr->vlenb,
                           data, fns[s->sew - MO_32]);
        finalize_rvv_inst(s);
        return true;
    }
    return false;
}
