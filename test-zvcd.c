/*
 * Runtime test for the experimental RISC-V Zvcd (Vector Conflict Detection)
 * extension implemented in QEMU.
 *
 * Zvcd defines two register-to-register cross-lane primitives (VMUNARY0 group,
 * funct6=010100, funct3=010):
 *   - vconflictcnt.v vd, vs2[, v0.t]   selector 10010, data result (SEW-bit)
 *   - vconflictlast.m vd, vs2[, v0.t]  selector 00100, mask result (EEW=1)
 *
 * The assembler does not yet know these mnemonics, so the instructions are
 * emitted as raw .word values matching the draft encoding:
 *   vconflictcnt.v  v16, v8        -> 0x52892857
 *   vconflictcnt.v  v16, v8, v0.t  -> 0x50892857
 *   vconflictlast.m v1,  v8        -> 0x528220d7
 *   vconflictlast.m v1,  v8, v0.t  -> 0x508220d7
 *
 * Build:
 *   riscv64-unknown-linux-gnu-gcc -O2 -static -march=rv64gcv test-zvcd.c -o test-zvcd
 * Run:
 *   qemu-riscv64 -cpu rv64,v=true,vlen=256,zvcd=true test-zvcd
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>

#define MAXN 16

static int failures;

#define CHECK(cond, fmt, ...)                                            \
    do {                                                                 \
        if (!(cond)) {                                                   \
            printf("FAIL: " fmt "\n", ##__VA_ARGS__);                    \
            failures++;                                                  \
        }                                                                \
    } while (0)

/* -------- reference models (serial, spec pseudocode) -------- */

static void ref_cnt(const uint64_t *vs2, const uint8_t *active,
                    uint64_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        if (!active[i]) {
            continue;
        }
        uint64_t count = 0;
        for (int j = 0; j <= i; j++) {
            if (active[j] && vs2[j] == vs2[i]) {
                count++;
            }
        }
        out[i] = count;
    }
}

static void ref_last(const uint64_t *vs2, const uint8_t *active,
                     uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        if (!active[i]) {
            continue;
        }
        int has_later = 0;
        for (int j = i + 1; j < n; j++) {
            if (active[j] && vs2[j] == vs2[i]) {
                has_later = 1;
                break;
            }
        }
        out[i] = !has_later;
    }
}

/* -------- device-under-test wrappers (SEW=32) -------- */

static int dut_cnt32(const uint32_t *src, uint32_t *dst, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %3, e32, m1, ta, ma\n\t"
        "vle32.v v8, (%1)\n\t"
        ".word 0x52892857\n\t"      /* vconflictcnt.v v16, v8 */
        "vse32.v v16, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

static int dut_cnt32_masked(const uint32_t *src, uint32_t *dst,
                            const uint8_t *mask, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %4, e32, m1, ta, ma\n\t"
        "vlm.v v0, (%3)\n\t"
        "vle32.v v8, (%1)\n\t"
        ".word 0x50892857\n\t"      /* vconflictcnt.v v16, v8, v0.t */
        "vse32.v v16, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"(mask), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

static int dut_last32(const uint32_t *src, uint8_t *dst, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %3, e32, m1, ta, ma\n\t"
        "vle32.v v8, (%1)\n\t"
        ".word 0x528220d7\n\t"      /* vconflictlast.m v1, v8 */
        "vsm.v v1, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

static int dut_last32_masked(const uint32_t *src, uint8_t *dst,
                             const uint8_t *mask, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %4, e32, m1, ta, ma\n\t"
        "vlm.v v0, (%3)\n\t"
        "vle32.v v8, (%1)\n\t"
        ".word 0x508220d7\n\t"      /* vconflictlast.m v1, v8, v0.t */
        "vsm.v v1, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"(mask), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

/* -------- device-under-test wrappers (SEW=64) -------- */

static int dut_cnt64(const uint64_t *src, uint64_t *dst, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %3, e64, m1, ta, ma\n\t"
        "vle64.v v8, (%1)\n\t"
        ".word 0x52892857\n\t"      /* vconflictcnt.v v16, v8 */
        "vse64.v v16, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

static int dut_last64(const uint64_t *src, uint8_t *dst, int n)
{
    long vl;
    asm volatile(
        "vsetvli %0, %3, e64, m1, ta, ma\n\t"
        "vle64.v v8, (%1)\n\t"
        ".word 0x528220d7\n\t"      /* vconflictlast.m v1, v8 */
        "vsm.v v1, (%2)\n\t"
        : "=r"(vl)
        : "r"(src), "r"(dst), "r"((long)n)
        : "t0", "memory");
    return (int)vl;
}

/* -------- helpers -------- */

static int mask_bit(const uint8_t *bytes, int i)
{
    return (bytes[i / 8] >> (i % 8)) & 1;
}

static void all_active(uint8_t *active, int n)
{
    for (int i = 0; i < n; i++) {
        active[i] = 1;
    }
}

/* -------- SEW=32 semantic tests -------- */

static void test_cnt32(const char *name, const uint32_t *src, int n)
{
    uint32_t got[MAXN];
    uint64_t wide[MAXN], ref[MAXN];
    uint8_t active[MAXN];
    int vl;

    memset(got, 0xAA, sizeof(got));
    all_active(active, n);
    for (int i = 0; i < n; i++) {
        wide[i] = src[i];
    }
    ref_cnt(wide, active, ref, n);
    vl = dut_cnt32(src, got, n);
    if (vl < n) {
        printf("SKIP: cnt32 %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        CHECK(got[i] == (uint32_t)ref[i],
              "cnt32 %s lane %d: got %u expected %u",
              name, i, got[i], (uint32_t)ref[i]);
    }
}

static void test_last32(const char *name, const uint32_t *src, int n)
{
    uint8_t got[8];
    uint64_t wide[MAXN];
    uint8_t ref[MAXN], active[MAXN];
    int vl;

    memset(got, 0, sizeof(got));
    all_active(active, n);
    for (int i = 0; i < n; i++) {
        wide[i] = src[i];
    }
    ref_last(wide, active, ref, n);
    vl = dut_last32(src, got, n);
    if (vl < n) {
        printf("SKIP: last32 %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        CHECK(mask_bit(got, i) == ref[i],
              "last32 %s lane %d: got %d expected %d",
              name, i, mask_bit(got, i), ref[i]);
    }
}

static void test_cnt32_masked(const char *name, const uint32_t *src,
                              const uint8_t *maskbits, int n)
{
    uint32_t got[MAXN];
    uint64_t wide[MAXN], ref[MAXN];
    uint8_t active[MAXN], mbytes[8];
    int vl;

    memset(got, 0, sizeof(got));
    memset(mbytes, 0, sizeof(mbytes));
    for (int i = 0; i < n; i++) {
        active[i] = maskbits[i];
        if (maskbits[i]) {
            mbytes[i / 8] |= 1u << (i % 8);
        }
        wide[i] = src[i];
    }
    ref_cnt(wide, active, ref, n);
    vl = dut_cnt32_masked(src, got, mbytes, n);
    if (vl < n) {
        printf("SKIP: cnt32-masked %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        if (active[i]) {
            CHECK(got[i] == (uint32_t)ref[i],
                  "cnt32-masked %s lane %d: got %u expected %u",
                  name, i, got[i], (uint32_t)ref[i]);
        }
    }
}

static void test_last32_masked(const char *name, const uint32_t *src,
                               const uint8_t *maskbits, int n)
{
    uint8_t got[8];
    uint64_t wide[MAXN];
    uint8_t ref[MAXN], active[MAXN], mbytes[8];
    int vl;

    memset(got, 0, sizeof(got));
    memset(mbytes, 0, sizeof(mbytes));
    for (int i = 0; i < n; i++) {
        active[i] = maskbits[i];
        if (maskbits[i]) {
            mbytes[i / 8] |= 1u << (i % 8);
        }
        wide[i] = src[i];
    }
    ref_last(wide, active, ref, n);
    vl = dut_last32_masked(src, got, mbytes, n);
    if (vl < n) {
        printf("SKIP: last32-masked %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        if (active[i]) {
            CHECK(mask_bit(got, i) == ref[i],
                  "last32-masked %s lane %d: got %d expected %d",
                  name, i, mask_bit(got, i), ref[i]);
        }
    }
}

/* -------- SEW=64 semantic tests -------- */

static void test_cnt64(const char *name, const uint64_t *src, int n)
{
    uint64_t got[MAXN], ref[MAXN];
    uint8_t active[MAXN];
    int vl;

    memset(got, 0xAA, sizeof(got));
    all_active(active, n);
    ref_cnt(src, active, ref, n);
    vl = dut_cnt64(src, got, n);
    if (vl < n) {
        printf("SKIP: cnt64 %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        CHECK(got[i] == ref[i],
              "cnt64 %s lane %d: got %llu expected %llu",
              name, i, (unsigned long long)got[i],
              (unsigned long long)ref[i]);
    }
}

static void test_last64(const char *name, const uint64_t *src, int n)
{
    uint8_t got[8], ref[MAXN], active[MAXN];
    int vl;

    memset(got, 0, sizeof(got));
    all_active(active, n);
    ref_last(src, active, ref, n);
    vl = dut_last64(src, got, n);
    if (vl < n) {
        printf("SKIP: last64 %s (VLMAX=%d < %d)\n", name, vl, n);
        return;
    }
    for (int i = 0; i < n; i++) {
        CHECK(mask_bit(got, i) == ref[i],
              "last64 %s lane %d: got %d expected %d",
              name, i, mask_bit(got, i), ref[i]);
    }
}

/* -------- vl=0 : no destination update -------- */

static void test_vl0(void)
{
    uint32_t src[4] = { 1, 2, 3, 4 };
    uint32_t cntdst[4];
    uint8_t lastdst[8];

    memset(cntdst, 0x5A, sizeof(cntdst));
    memset(lastdst, 0x3C, sizeof(lastdst));

    dut_cnt32(src, cntdst, 0);
    dut_last32(src, lastdst, 0);

    for (int i = 0; i < 4; i++) {
        CHECK(cntdst[i] == 0x5A5A5A5Au,
              "vl0 cnt dst lane %d modified: 0x%08x", i, cntdst[i]);
    }
    /* vsm.v with evl=0 stores nothing */
    CHECK(lastdst[0] == 0x3C, "vl0 last dst modified: 0x%02x", lastdst[0]);
}

/* -------- illegal SEW=8 must trap -------- */

static sigjmp_buf sigill_env;
static volatile int got_sigill;

static void sigill_handler(int sig)
{
    (void)sig;
    got_sigill = 1;
    siglongjmp(sigill_env, 1);
}

static void test_illegal_sew8(void)
{
    struct sigaction sa, old;
    uint32_t src[4] = { 1, 1, 2, 2 };
    uint32_t dst[4] = { 0 };

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigill_handler;
    sigaction(SIGILL, &sa, &old);

    got_sigill = 0;
    if (sigsetjmp(sigill_env, 1) == 0) {
        asm volatile(
            "vsetvli t0, %2, e8, m1, ta, ma\n\t"
            "vle8.v v8, (%0)\n\t"
            ".word 0x52892857\n\t"  /* vconflictcnt.v v16, v8 at SEW=8 */
            "vse8.v v16, (%1)\n\t"
            :
            : "r"(src), "r"(dst), "r"(4L)
            : "t0", "memory");
    }
    sigaction(SIGILL, &old, NULL);

    CHECK(got_sigill, "SEW=8 vconflictcnt.v did not raise illegal instruction");
}

int main(void)
{
    /* Normative example: vs2 = [7, 3, 7, 7, 3] */
    static const uint32_t ex32[5] = { 7, 3, 7, 7, 3 };
    static const uint64_t ex64[5] = { 7, 3, 7, 7, 3 };

    /* all-unique */
    static const uint32_t uniq[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
    /* all-equal */
    static const uint32_t equal[6] = { 9, 9, 9, 9, 9, 9 };
    /* multiple duplicate groups */
    static const uint32_t groups[8] = { 5, 6, 5, 7, 6, 5, 8, 7 };

    /* mask [1,1,1,0,1] applied to the normative example */
    static const uint8_t maskA[5] = { 1, 1, 1, 0, 1 };

    printf("== Zvcd runtime tests ==\n");

    test_cnt32("normative", ex32, 5);
    test_last32("normative", ex32, 5);

    test_cnt32("unique", uniq, 8);
    test_last32("unique", uniq, 8);

    test_cnt32("equal", equal, 6);
    test_last32("equal", equal, 6);

    test_cnt32("groups", groups, 8);
    test_last32("groups", groups, 8);

    test_cnt32_masked("normative", ex32, maskA, 5);
    test_last32_masked("normative", ex32, maskA, 5);

    test_cnt64("normative", ex64, 5);
    test_last64("normative", ex64, 5);

    test_vl0();
    test_illegal_sew8();

    if (failures == 0) {
        printf("PASS: all Zvcd tests passed\n");
        return 0;
    }
    printf("FAILED: %d check(s)\n", failures);
    return 1;
}
