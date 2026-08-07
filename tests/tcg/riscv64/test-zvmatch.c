#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define MAX_ELEMS 1024
#define MAX_MASK_BYTES (MAX_ELEMS / 8)

#define VMMATCH_VV(vd, vs2, vs1, vm) \
    ((0x34u << 26) | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | \
     ((vd) << 7) | 0x57u)
#define VMMATCH_VX(vd, vs2, rs1, vm) \
    ((0x34u << 26) | ((vm) << 25) | ((vs2) << 20) | ((rs1) << 15) | \
     (4u << 12) | ((vd) << 7) | 0x57u)

typedef size_t (*vv_runner)(uint8_t *, const uint8_t *, const void *,
                            const void *, size_t, size_t);

static sigjmp_buf sigill_env;
static volatile sig_atomic_t saw_sigill;

static int mask_bit(const uint8_t *mask, size_t index)
{
    return (mask[index / 8] >> (index % 8)) & 1;
}

static void set_mask_bit(uint8_t *mask, size_t index, int value)
{
    uint8_t bit = 1u << (index % 8);

    if (value) {
        mask[index / 8] |= bit;
    } else {
        mask[index / 8] &= ~bit;
    }
}

static void sigill_handler(int sig)
{
    if (sig != SIGILL) {
        _exit(1);
    }
    saw_sigill = 1;
    siglongjmp(sigill_env, 1);
}

static void expect_sigill(void (*test)(void))
{
    saw_sigill = 0;
    if (sigsetjmp(sigill_env, 1) == 0) {
        test();
        _exit(1);
    }
    assert(saw_sigill);
}

#define DEFINE_VV_RUNNER(NAME, VTYPE, LOAD)                              \
static size_t run_vv_##NAME(uint8_t *out, const uint8_t *initial,        \
                            const void *query, const void *keys,          \
                            size_t vl, size_t start)                      \
{                                                                        \
    size_t vlmax;                                                        \
    size_t final_start;                                                  \
                                                                         \
    asm volatile(                                                        \
        "li t0, -1\n\t"                                                  \
        "vsetvli %[vlmax], t0, " VTYPE ", tu, mu\n\t"                   \
        "vlm.v v1, (%[initial])\n\t"                                     \
        LOAD " v8, (%[query])\n\t"                                       \
        LOAD " v16, (%[keys])\n\t"                                       \
        "vsetvli zero, %[vl], " VTYPE ", tu, mu\n\t"                    \
        "csrw vstart, %[start]\n\t"                                      \
        ".word %[insn]\n\t"                                              \
        "csrr %[final_start], vstart\n\t"                                \
        "li t0, -1\n\t"                                                  \
        "vsetvli zero, t0, " VTYPE ", tu, mu\n\t"                       \
        "vsm.v v1, (%[out])"                                             \
        : [vlmax] "=&r"(vlmax), [final_start] "=&r"(final_start)         \
        : [out] "r"(out), [initial] "r"(initial), [query] "r"(query),   \
          [keys] "r"(keys), [vl] "r"(vl), [start] "r"(start),           \
          [insn] "i"(VMMATCH_VV(1, 8, 16, 1))                           \
        : "t0", "memory");                                               \
    assert(final_start == 0);                                            \
    return vlmax;                                                        \
}

DEFINE_VV_RUNNER(e8mf8, "e8, mf8", "vle8.v")
DEFINE_VV_RUNNER(e8mf4, "e8, mf4", "vle8.v")
DEFINE_VV_RUNNER(e8mf2, "e8, mf2", "vle8.v")
DEFINE_VV_RUNNER(e8m1, "e8, m1", "vle8.v")
DEFINE_VV_RUNNER(e8m2, "e8, m2", "vle8.v")
DEFINE_VV_RUNNER(e8m4, "e8, m4", "vle8.v")
DEFINE_VV_RUNNER(e8m8, "e8, m8", "vle8.v")
DEFINE_VV_RUNNER(e16mf4, "e16, mf4", "vle16.v")
DEFINE_VV_RUNNER(e16mf2, "e16, mf2", "vle16.v")
DEFINE_VV_RUNNER(e16m1, "e16, m1", "vle16.v")
DEFINE_VV_RUNNER(e16m2, "e16, m2", "vle16.v")
DEFINE_VV_RUNNER(e16m4, "e16, m4", "vle16.v")
DEFINE_VV_RUNNER(e16m8, "e16, m8", "vle16.v")

#define DEFINE_VX_RUNNER(NAME, VTYPE, LOAD, RS1)                         \
static size_t run_vx_##NAME(uint8_t *out, const uint8_t *initial,        \
                            const void *query, uintptr_t keys,            \
                            size_t vl, size_t start)                      \
{                                                                        \
    size_t vlmax;                                                        \
    size_t final_start;                                                  \
                                                                         \
    asm volatile(                                                        \
        "li t0, -1\n\t"                                                  \
        "vsetvli %[vlmax], t0, " VTYPE ", tu, mu\n\t"                   \
        "vlm.v v1, (%[initial])\n\t"                                     \
        LOAD " v8, (%[query])\n\t"                                       \
        "vsetvli zero, %[vl], " VTYPE ", tu, mu\n\t"                    \
        "mv a6, %[keys]\n\t"                                             \
        "csrw vstart, %[start]\n\t"                                      \
        ".word %[insn]\n\t"                                              \
        "csrr %[final_start], vstart\n\t"                                \
        "li t0, -1\n\t"                                                  \
        "vsetvli zero, t0, " VTYPE ", tu, mu\n\t"                       \
        "vsm.v v1, (%[out])"                                             \
        : [vlmax] "=&r"(vlmax), [final_start] "=&r"(final_start)         \
        : [out] "r"(out), [initial] "r"(initial), [query] "r"(query),   \
          [keys] "r"(keys), [vl] "r"(vl), [start] "r"(start),           \
          [insn] "i"(VMMATCH_VX(1, 8, RS1, 1))                          \
        : "a6", "t0", "memory");                                        \
    assert(final_start == 0);                                            \
    return vlmax;                                                        \
}

DEFINE_VX_RUNNER(e8, "e8, m1", "vle8.v", 16)
DEFINE_VX_RUNNER(e16, "e16, m1", "vle16.v", 16)
DEFINE_VX_RUNNER(e8x0, "e8, m1", "vle8.v", 0)

#define DEFINE_MASKED_RUNNER(NAME, TAIL_POLICY, MASK_POLICY)             \
static size_t run_masked_##NAME(uint8_t *out, const uint8_t *initial,    \
                                const uint8_t *exec_mask,                 \
                                const uint8_t *query, const uint8_t *keys,\
                                size_t vl)                               \
{                                                                        \
    size_t vlmax;                                                        \
                                                                         \
    asm volatile(                                                        \
        "li t0, -1\n\t"                                                  \
        "vsetvli %[vlmax], t0, e8, m1, tu, mu\n\t"                      \
        "vlm.v v1, (%[initial])\n\t"                                     \
        "vlm.v v0, (%[exec_mask])\n\t"                                   \
        "vle8.v v8, (%[query])\n\t"                                      \
        "vle8.v v16, (%[keys])\n\t"                                      \
        "vsetvli zero, %[vl], e8, m1, " TAIL_POLICY ", " MASK_POLICY    \
        "\n\t"                                                           \
        ".word %[insn]\n\t"                                              \
        "li t0, -1\n\t"                                                  \
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"                          \
        "vsm.v v1, (%[out])"                                             \
        : [vlmax] "=&r"(vlmax)                                          \
        : [out] "r"(out), [initial] "r"(initial),                       \
          [exec_mask] "r"(exec_mask), [query] "r"(query),               \
          [keys] "r"(keys), [vl] "r"(vl),                               \
          [insn] "i"(VMMATCH_VV(1, 8, 16, 0))                           \
        : "t0", "memory");                                               \
    return vlmax;                                                        \
}

DEFINE_MASKED_RUNNER(tu_mu, "tu", "mu")
DEFINE_MASKED_RUNNER(tu_ma, "tu", "ma")
DEFINE_MASKED_RUNNER(ta_mu, "ta", "mu")
DEFINE_MASKED_RUNNER(ta_ma, "ta", "ma")

static size_t run_masked_vd_v0(uint8_t *out, const uint8_t *exec_mask,
                               const uint8_t *query, uintptr_t keys,
                               size_t vl)
{
    size_t vlmax;

    asm volatile(
        "li t0, -1\n\t"
        "vsetvli %[vlmax], t0, e8, m1, tu, mu\n\t"
        "vlm.v v0, (%[exec_mask])\n\t"
        "vle8.v v8, (%[query])\n\t"
        "vsetvli zero, %[vl], e8, m1, tu, mu\n\t"
        "mv a6, %[keys]\n\t"
        ".word %[insn]\n\t"
        "li t0, -1\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vsm.v v0, (%[out])"
        : [vlmax] "=&r"(vlmax)
        : [out] "r"(out), [exec_mask] "r"(exec_mask),
          [query] "r"(query), [keys] "r"(keys), [vl] "r"(vl),
          [insn] "i"(VMMATCH_VX(0, 8, 16, 0))
        : "a6", "t0", "memory");
    return vlmax;
}

static size_t run_masked_vv_vd_v0(uint8_t *out, const uint8_t *exec_mask,
                                  const uint8_t *query, const uint8_t *keys,
                                  size_t vl)
{
    size_t vlmax;

    asm volatile(
        "li t0, -1\n\t"
        "vsetvli %[vlmax], t0, e8, m1, tu, mu\n\t"
        "vlm.v v0, (%[exec_mask])\n\t"
        "vle8.v v8, (%[query])\n\t"
        "vle8.v v16, (%[keys])\n\t"
        "vsetvli zero, %[vl], e8, m1, tu, mu\n\t"
        ".word %[insn]\n\t"
        "li t0, -1\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vsm.v v0, (%[out])"
        : [vlmax] "=&r"(vlmax)
        : [out] "r"(out), [exec_mask] "r"(exec_mask),
          [query] "r"(query), [keys] "r"(keys), [vl] "r"(vl),
          [insn] "i"(VMMATCH_VV(0, 8, 16, 0))
        : "t0", "memory");
    return vlmax;
}

static size_t run_vv_same_sources(uint8_t *out, const uint8_t *source,
                                  size_t vl)
{
    size_t vlmax;

    asm volatile(
        "li t0, -1\n\t"
        "vsetvli %[vlmax], t0, e8, m1, tu, mu\n\t"
        "vle8.v v8, (%[source])\n\t"
        "vsetvli zero, %[vl], e8, m1, tu, mu\n\t"
        ".word %[insn]\n\t"
        "li t0, -1\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vsm.v v1, (%[out])"
        : [vlmax] "=&r"(vlmax)
        : [out] "r"(out), [source] "r"(source), [vl] "r"(vl),
          [insn] "i"(VMMATCH_VV(1, 8, 8, 1))
        : "t0", "memory");
    return vlmax;
}

static size_t run_overlap_vx(uint8_t *out, const uint8_t *query,
                             uintptr_t keys, size_t vl)
{
    size_t vlmax;

    asm volatile(
        "li t0, -1\n\t"
        "vsetvli %[vlmax], t0, e8, m2, tu, mu\n\t"
        "vle8.v v8, (%[query])\n\t"
        "vsetvli zero, %[vl], e8, m2, tu, mu\n\t"
        "mv a6, %[keys]\n\t"
        ".word %[insn]\n\t"
        "li t0, -1\n\t"
        "vsetvli zero, t0, e8, m2, tu, mu\n\t"
        "vsm.v v8, (%[out])"
        : [vlmax] "=&r"(vlmax)
        : [out] "r"(out), [query] "r"(query), [keys] "r"(keys),
          [vl] "r"(vl), [insn] "i"(VMMATCH_VX(8, 8, 16, 1))
        : "a6", "t0", "memory");
    return vlmax;
}

static size_t run_overlap_vv(uint8_t *out, const uint8_t *query,
                             const uint8_t *keys, size_t vl)
{
    size_t vlmax;

    asm volatile(
        "li t0, -1\n\t"
        "vsetvli %[vlmax], t0, e8, m2, tu, mu\n\t"
        "vle8.v v8, (%[query])\n\t"
        "vle8.v v16, (%[keys])\n\t"
        "vsetvli zero, %[vl], e8, m2, tu, mu\n\t"
        ".word %[insn]\n\t"
        "li t0, -1\n\t"
        "vsetvli zero, t0, e8, m2, tu, mu\n\t"
        "vsm.v v8, (%[out])"
        : [vlmax] "=&r"(vlmax)
        : [out] "r"(out), [query] "r"(query), [keys] "r"(keys),
          [vl] "r"(vl), [insn] "i"(VMMATCH_VV(8, 8, 16, 1))
        : "t0", "memory");
    return vlmax;
}

static void check_vv_e8_lmul(vv_runner run)
{
    uint8_t initial[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t query[MAX_ELEMS] __attribute__((aligned(16)));
    uint8_t keys[MAX_ELEMS] __attribute__((aligned(16)));
    size_t vlmax;
    size_t test_vls[4];

    memset(initial, 0x5a, sizeof(initial));
    memset(query, 0x44, sizeof(query));
    memset(keys, 0x7a, sizeof(keys));
    memset(out, 0, sizeof(out));
    vlmax = run(out, initial, query, keys, 0, 0);
    assert(vlmax > 0 && vlmax <= MAX_ELEMS);
    keys[vlmax - 1] = 0x33;
    query[0] = 0x33;
    test_vls[0] = 0;
    test_vls[1] = 1;
    test_vls[2] = vlmax - 1;
    test_vls[3] = vlmax;

    for (size_t n = 0; n < 4; n++) {
        size_t vl = test_vls[n];

        memset(out, 0, sizeof(out));
        assert(run(out, initial, query, keys, vl, 0) == vlmax);
        for (size_t i = 0; i < vlmax; i++) {
            int expected;

            if (vl == 0) {
                expected = mask_bit(initial, i);
            } else if (i < vl) {
                expected = i == 0;
            } else {
                expected = 1;
            }
            assert(mask_bit(out, i) == expected);
        }
    }
}

static void check_vv_e16_lmul(vv_runner run)
{
    uint8_t initial[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint16_t query[MAX_ELEMS] __attribute__((aligned(16)));
    uint16_t keys[MAX_ELEMS] __attribute__((aligned(16)));
    size_t vlmax;
    size_t test_vls[4];

    memset(initial, 0xa5, sizeof(initial));
    for (size_t i = 0; i < MAX_ELEMS; i++) {
        query[i] = 0x4444;
        keys[i] = 0x7a7a;
    }
    memset(out, 0, sizeof(out));
    vlmax = run(out, initial, query, keys, 0, 0);
    assert(vlmax > 0 && vlmax <= MAX_ELEMS);
    keys[vlmax - 1] = 0x3333;
    query[0] = 0x3333;
    test_vls[0] = 0;
    test_vls[1] = 1;
    test_vls[2] = vlmax - 1;
    test_vls[3] = vlmax;

    for (size_t n = 0; n < 4; n++) {
        size_t vl = test_vls[n];

        memset(out, 0, sizeof(out));
        assert(run(out, initial, query, keys, vl, 0) == vlmax);
        for (size_t i = 0; i < vlmax; i++) {
            int expected;

            if (vl == 0) {
                expected = mask_bit(initial, i);
            } else if (i < vl) {
                expected = i == 0;
            } else {
                expected = 1;
            }
            assert(mask_bit(out, i) == expected);
        }
    }
}

static void test_all_lmul(void)
{
    static vv_runner const e8_runners[] = {
        run_vv_e8mf8, run_vv_e8mf4, run_vv_e8mf2, run_vv_e8m1,
        run_vv_e8m2, run_vv_e8m4, run_vv_e8m8,
    };
    static vv_runner const e16_runners[] = {
        run_vv_e16mf4, run_vv_e16mf2, run_vv_e16m1,
        run_vv_e16m2, run_vv_e16m4, run_vv_e16m8,
    };

    for (size_t i = 0; i < sizeof(e8_runners) / sizeof(e8_runners[0]); i++) {
        check_vv_e8_lmul(e8_runners[i]);
    }
    for (size_t i = 0; i < sizeof(e16_runners) / sizeof(e16_runners[0]);
         i++) {
        check_vv_e16_lmul(e16_runners[i]);
    }
}

static void test_vx(void)
{
    uint8_t initial[MAX_MASK_BYTES] __attribute__((aligned(16))) = { 0 };
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t query8[MAX_ELEMS] __attribute__((aligned(16))) = {
        0x11, 0x88, 0x44, 0x99, 0x00, 0xff,
    };
    uint16_t query16[MAX_ELEMS] __attribute__((aligned(16))) = {
        0x1111, 0x4444, 0x2222, 0x9999, 0x0000,
    };
    size_t vlmax;

    memset(out, 0, sizeof(out));
    vlmax = run_vx_e8(out, initial, query8, UINT64_C(0x8877665544332211),
                      5, 0);
    assert(vlmax >= 5);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(mask_bit(out, 2));
    assert(!mask_bit(out, 3));
    assert(!mask_bit(out, 4));

    memset(out, 0, sizeof(out));
    assert(run_vx_e8(out, initial, query8, UINT64_MAX, 6, 0) == vlmax);
    for (size_t i = 0; i < 5; i++) {
        assert(!mask_bit(out, i));
    }
    assert(mask_bit(out, 5));

    memset(out, 0, sizeof(out));
    vlmax = run_vx_e16(out, initial, query16,
                       UINT64_C(0x4444333322221111), 5, 0);
    assert(vlmax >= 5);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(mask_bit(out, 2));
    assert(!mask_bit(out, 3));
    assert(!mask_bit(out, 4));

    memset(out, 0, sizeof(out));
    vlmax = run_vx_e8x0(out, initial, query8, UINT64_MAX, 5, 0);
    assert(vlmax >= 5);
    for (size_t i = 0; i < 4; i++) {
        assert(!mask_bit(out, i));
    }
    assert(mask_bit(out, 4));
}

static void test_mask_policy(void)
{
    uint8_t initial[MAX_MASK_BYTES] __attribute__((aligned(16))) = { 0 };
    uint8_t exec_mask[MAX_MASK_BYTES] __attribute__((aligned(16))) = { 0 };
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t query[MAX_ELEMS] __attribute__((aligned(16))) = {
        0x42, 0x42, 0x99, 0x99,
    };
    uint8_t keys[MAX_ELEMS] __attribute__((aligned(16)));
    size_t vlmax;

    memset(keys, 0x7a, sizeof(keys));
    keys[1] = 0x42;
    set_mask_bit(initial, 1, 1);
    set_mask_bit(initial, 3, 1);
    set_mask_bit(exec_mask, 0, 1);
    set_mask_bit(exec_mask, 2, 1);

    memset(out, 0, sizeof(out));
    vlmax = run_masked_tu_mu(out, initial, exec_mask, query, keys, 4);
    assert(vlmax >= 4);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(mask_bit(out, 3));
    for (size_t i = 4; i < vlmax; i++) {
        assert(mask_bit(out, i));
    }

    memset(out, 0, sizeof(out));
    assert(run_masked_ta_mu(out, initial, exec_mask, query, keys, 4) ==
           vlmax);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(mask_bit(out, 3));
    for (size_t i = 4; i < vlmax; i++) {
        assert(mask_bit(out, i));
    }

    memset(initial, 0, sizeof(initial));
    memset(out, 0, sizeof(out));
    assert(run_masked_tu_ma(out, initial, exec_mask, query, keys, 4) ==
           vlmax);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(mask_bit(out, 3));

    memset(out, 0, sizeof(out));
    assert(run_masked_ta_ma(out, initial, exec_mask, query, keys, 4) ==
           vlmax);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(mask_bit(out, 3));

    memset(out, 0, sizeof(out));
    assert(run_masked_vd_v0(out, exec_mask, query,
                            UINT64_C(0x4242424242424242), 4) == vlmax);
    assert(mask_bit(out, 0));
    assert(!mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(!mask_bit(out, 3));

    memset(out, 0, sizeof(out));
    assert(run_masked_vv_vd_v0(out, exec_mask, query, keys, 4) == vlmax);
    assert(mask_bit(out, 0));
    assert(!mask_bit(out, 1));
    assert(!mask_bit(out, 2));
    assert(!mask_bit(out, 3));
}

static void test_vstart_and_full_key_domain(void)
{
    uint8_t initial[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t query[MAX_ELEMS] __attribute__((aligned(16)));
    uint8_t keys[MAX_ELEMS] __attribute__((aligned(16)));
    size_t vlmax;

    memset(initial, 0, sizeof(initial));
    memset(query, 0x44, sizeof(query));
    memset(keys, 0x7a, sizeof(keys));
    set_mask_bit(initial, 0, 1);
    query[2] = 0x33;
    vlmax = run_vv_e8m1(out, initial, query, keys, 0, 0);
    assert(vlmax >= 4);
    keys[vlmax - 1] = 0x33;

    memset(out, 0, sizeof(out));
    assert(run_vv_e8m1(out, initial, query, keys, 4, 2) == vlmax);
    assert(mask_bit(out, 0));
    assert(!mask_bit(out, 1));
    assert(mask_bit(out, 2));
    assert(!mask_bit(out, 3));
    for (size_t i = 4; i < vlmax; i++) {
        assert(mask_bit(out, i));
    }

    memset(out, 0, sizeof(out));
    assert(run_vv_e8m1(out, initial, query, keys, 4, 4) == vlmax);
    for (size_t i = 0; i < vlmax; i++) {
        assert(mask_bit(out, i) == mask_bit(initial, i));
    }
}

static void test_legal_overlap(void)
{
    uint8_t out[MAX_MASK_BYTES] __attribute__((aligned(16)));
    uint8_t query[MAX_ELEMS] __attribute__((aligned(16)));
    uint8_t keys[MAX_ELEMS] __attribute__((aligned(16)));
    size_t vlmax;

    memset(query, 0x99, sizeof(query));
    memset(keys, 0x7a, sizeof(keys));
    query[0] = 0x11;
    query[1] = 0x22;
    query[2] = 0x99;
    keys[0] = 0x11;
    keys[1] = 0x22;

    memset(out, 0, sizeof(out));
    vlmax = run_overlap_vx(out, query, UINT64_C(0x1111111111111111), 3);
    assert(vlmax >= 3);
    assert(mask_bit(out, 0));
    assert(!mask_bit(out, 1));
    assert(!mask_bit(out, 2));

    memset(out, 0, sizeof(out));
    assert(run_overlap_vv(out, query, keys, 3) == vlmax);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(!mask_bit(out, 2));

    memset(out, 0, sizeof(out));
    assert(run_vv_same_sources(out, query, 3) == vlmax / 2);
    assert(mask_bit(out, 0));
    assert(mask_bit(out, 1));
    assert(mask_bit(out, 2));
}

static void try_vx_e32(void)
{
    asm volatile(
        "vsetivli zero, 1, e32, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VX(1, 8, 16, 1))
        : "memory");
}

static void try_vv_e64(void)
{
    asm volatile(
        "vsetivli zero, 1, e64, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VV(1, 8, 16, 1))
        : "memory");
}

static void try_vx_bad_overlap(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m2, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VX(9, 8, 16, 1))
        : "memory");
}

static void try_vv_bad_query_overlap(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m2, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VV(9, 8, 16, 1))
        : "memory");
}

static void try_vv_key_overlap(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VV(16, 8, 16, 1))
        : "memory");
}

static void try_vx_mask_source_v0(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VX(1, 0, 16, 0))
        : "memory");
}

static void try_vv_mask_query_v0(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VV(1, 0, 16, 0))
        : "memory");
}

static void try_vv_mask_keys_v0(void)
{
    asm volatile(
        "vsetivli zero, 1, e8, m1, ta, ma\n\t"
        ".word %[insn]"
        :
        : [insn] "i"(VMMATCH_VV(1, 8, 0, 0))
        : "memory");
}

static void test_reserved_encodings(void)
{
    expect_sigill(try_vx_e32);
    expect_sigill(try_vv_e64);
    expect_sigill(try_vx_bad_overlap);
    expect_sigill(try_vv_bad_query_overlap);
    expect_sigill(try_vv_key_overlap);
    expect_sigill(try_vx_mask_source_v0);
    expect_sigill(try_vv_mask_query_v0);
    expect_sigill(try_vv_mask_keys_v0);
}

int main(void)
{
    struct sigaction action = {
        .sa_handler = sigill_handler,
    };

    assert(sigemptyset(&action.sa_mask) == 0);
    assert(sigaction(SIGILL, &action, NULL) == 0);

    test_vx();
    test_all_lmul();
    test_mask_policy();
    test_vstart_and_full_key_domain();
    test_legal_overlap();
    test_reserved_encodings();
    return 0;
}
