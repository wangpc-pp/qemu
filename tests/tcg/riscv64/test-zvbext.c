#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static uint64_t ref_compress(uint64_t value, uint64_t mask)
{
    uint64_t result = 0;
    uint64_t result_bit = 1;

    while (mask) {
        uint64_t mask_bit = mask & -mask;

        if (value & mask_bit) {
            result |= result_bit;
        }
        mask &= mask - 1;
        result_bit <<= 1;
    }
    return result;
}

static uint64_t ref_expand(uint64_t value, uint64_t mask)
{
    uint64_t result = 0;
    uint64_t value_bit = 1;

    while (mask) {
        uint64_t mask_bit = mask & -mask;

        if (value & value_bit) {
            result |= mask_bit;
        }
        mask &= mask - 1;
        value_bit <<= 1;
    }
    return result;
}

#define DEFINE_RUNNERS(SUFFIX, TYPE, VTYPE, LOAD, STORE)                 \
static void compress_vv_##SUFFIX(TYPE out[], const TYPE data[],          \
                                 const TYPE mask[], size_t vl)           \
{                                                                        \
    asm volatile(                                                        \
        "vsetvli zero, %[vl], " VTYPE ", m1, tu, mu\n\t"                \
        LOAD " v9, (%[data])\n\t"                                        \
        LOAD " v8, (%[mask])\n\t"                                        \
        ".word 0xca940557\n\t"                                           \
        STORE " v10, (%[out])"                                           \
        :                                                                \
        : [out] "r"(out), [data] "r"(data), [mask] "r"(mask),           \
          [vl] "r"(vl)                                                   \
        : "memory");                                                     \
}                                                                        \
                                                                         \
static void expand_vv_##SUFFIX(TYPE out[], const TYPE data[],            \
                               const TYPE mask[], size_t vl)             \
{                                                                        \
    asm volatile(                                                        \
        "vsetvli zero, %[vl], " VTYPE ", m1, tu, mu\n\t"                \
        LOAD " v9, (%[data])\n\t"                                        \
        LOAD " v8, (%[mask])\n\t"                                        \
        ".word 0xce940557\n\t"                                           \
        STORE " v10, (%[out])"                                           \
        :                                                                \
        : [out] "r"(out), [data] "r"(data), [mask] "r"(mask),           \
          [vl] "r"(vl)                                                   \
        : "memory");                                                     \
}                                                                        \
                                                                         \
static void compress_vx_##SUFFIX(TYPE out[], const TYPE data[],          \
                                 uint64_t mask, size_t vl)               \
{                                                                        \
    asm volatile(                                                        \
        "vsetvli zero, %[vl], " VTYPE ", m1, tu, mu\n\t"                \
        LOAD " v9, (%[data])\n\t"                                        \
        "mv a6, %[mask]\n\t"                                             \
        ".word 0xca984557\n\t"                                           \
        STORE " v10, (%[out])"                                           \
        :                                                                \
        : [out] "r"(out), [data] "r"(data), [mask] "r"(mask),           \
          [vl] "r"(vl)                                                   \
        : "a6", "memory");                                               \
}                                                                        \
                                                                         \
static void expand_vx_##SUFFIX(TYPE out[], const TYPE data[],            \
                               uint64_t mask, size_t vl)                 \
{                                                                        \
    asm volatile(                                                        \
        "vsetvli zero, %[vl], " VTYPE ", m1, tu, mu\n\t"                \
        LOAD " v9, (%[data])\n\t"                                        \
        "mv a6, %[mask]\n\t"                                             \
        ".word 0xce984557\n\t"                                           \
        STORE " v10, (%[out])"                                           \
        :                                                                \
        : [out] "r"(out), [data] "r"(data), [mask] "r"(mask),           \
          [vl] "r"(vl)                                                   \
        : "a6", "memory");                                               \
}

DEFINE_RUNNERS(e8, uint8_t, "e8", "vle8.v", "vse8.v")
DEFINE_RUNNERS(e16, uint16_t, "e16", "vle16.v", "vse16.v")
DEFINE_RUNNERS(e32, uint32_t, "e32", "vle32.v", "vse32.v")
DEFINE_RUNNERS(e64, uint64_t, "e64", "vle64.v", "vse64.v")

#define CHECK_WIDTH(SUFFIX, TYPE, DATA, MASK, SCALAR_MASK)               \
do {                                                                     \
    TYPE out[ARRAY_SIZE(DATA)] __attribute__((aligned(16)));             \
                                                                         \
    compress_vv_##SUFFIX(out, DATA, MASK, ARRAY_SIZE(DATA));             \
    for (size_t i = 0; i < ARRAY_SIZE(DATA); i++) {                      \
        assert(out[i] == (TYPE)ref_compress(DATA[i], MASK[i]));          \
    }                                                                    \
    expand_vv_##SUFFIX(out, DATA, MASK, ARRAY_SIZE(DATA));               \
    for (size_t i = 0; i < ARRAY_SIZE(DATA); i++) {                      \
        assert(out[i] == (TYPE)ref_expand(DATA[i], MASK[i]));            \
    }                                                                    \
    compress_vx_##SUFFIX(out, DATA, SCALAR_MASK, ARRAY_SIZE(DATA));      \
    for (size_t i = 0; i < ARRAY_SIZE(DATA); i++) {                      \
        assert(out[i] == (TYPE)ref_compress(DATA[i], SCALAR_MASK));      \
    }                                                                    \
    expand_vx_##SUFFIX(out, DATA, SCALAR_MASK, ARRAY_SIZE(DATA));        \
    for (size_t i = 0; i < ARRAY_SIZE(DATA); i++) {                      \
        assert(out[i] == (TYPE)ref_expand(DATA[i], SCALAR_MASK));        \
    }                                                                    \
} while (0)

static void test_widths(void)
{
    static const uint8_t data8[] __attribute__((aligned(16))) = {
        0xaa, 0xf0, 0x81, 0x55
    };
    static const uint8_t mask8[] __attribute__((aligned(16))) = {
        0xcc, 0x0f, 0x81, 0xaa
    };
    static const uint16_t data16[] __attribute__((aligned(16))) = {
        0xa55a, 0xf00f, 0x8001, 0x5aa5
    };
    static const uint16_t mask16[] __attribute__((aligned(16))) = {
        0xcc33, 0x0f0f, 0x8001, 0xaaaa
    };
    static const uint32_t data32[] __attribute__((aligned(16))) = {
        0xa55aa55a, 0xf000000f, 0x80000001, 0x5aa55aa5
    };
    static const uint32_t mask32[] __attribute__((aligned(16))) = {
        0xcc3300ff, 0x0f0f0f0f, 0x80000001, 0xaaaaaaaa
    };
    static const uint64_t data64[] __attribute__((aligned(16))) = {
        UINT64_C(0xa55aa55a5aa55aa5),
        UINT64_C(0xf00000000000000f)
    };
    static const uint64_t mask64[] __attribute__((aligned(16))) = {
        UINT64_C(0xcc3300ffcc3300ff),
        UINT64_C(0x0f0f0f0f0f0f0f0f)
    };

    CHECK_WIDTH(e8, uint8_t, data8, mask8, UINT64_C(0xcc));
    CHECK_WIDTH(e16, uint16_t, data16, mask16, UINT64_C(0xcc33));
    CHECK_WIDTH(e32, uint32_t, data32, mask32, UINT64_C(0xcc3300ff));
    CHECK_WIDTH(e64, uint64_t, data64, mask64,
                UINT64_C(0xcc3300ffcc3300ff));
}

static void test_spec_example(void)
{
    static const uint8_t data[] __attribute__((aligned(16))) = {
        0xaa, 0xa5, 0xa5
    };
    static const uint8_t mask[] __attribute__((aligned(16))) = {
        0xcc, 0x00, 0xff
    };
    uint8_t out[ARRAY_SIZE(data)] __attribute__((aligned(16)));

    compress_vv_e8(out, data, mask, ARRAY_SIZE(data));
    assert(out[0] == 0x0a);
    assert(out[1] == 0x00);
    assert(out[2] == data[2]);
    expand_vv_e8(out, out, mask, ARRAY_SIZE(data));
    assert(out[0] == 0x88);
    assert(out[1] == 0x00);
    assert(out[2] == data[2]);
}

static void run_masked(uint8_t *out, const uint8_t *initial,
                       const uint8_t *data, const uint8_t *mask,
                       const uint8_t *predicate)
{
    asm volatile(
        "li t0, 16\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vle8.v v10, (%[initial])\n\t"
        "li t0, 4\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vle8.v v9, (%[data])\n\t"
        "vle8.v v8, (%[mask])\n\t"
        "vlm.v v0, (%[predicate])\n\t"
        ".word 0xc8940557\n\t"
        "li t0, 16\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vse8.v v10, (%[out])"
        :
        : [out] "r"(out), [initial] "r"(initial), [data] "r"(data),
          [mask] "r"(mask), [predicate] "r"(predicate)
        : "t0", "memory");
}

static void test_mask_and_tail(void)
{
    static const uint8_t initial[16] __attribute__((aligned(16))) = {
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f
    };
    static const uint8_t data[16] __attribute__((aligned(16))) = {
        0xaa, 0xf0, 0x81, 0x55
    };
    static const uint8_t mask[16] __attribute__((aligned(16))) = {
        0xcc, 0x0f, 0x81, 0xaa
    };
    static const uint8_t predicate[] = { 0x05 };
    uint8_t out[16] __attribute__((aligned(16)));

    run_masked(out, initial, data, mask, predicate);
    assert(out[0] == ref_compress(data[0], mask[0]));
    assert(out[2] == ref_compress(data[2], mask[2]));
    for (size_t i = 0; i < ARRAY_SIZE(out); i++) {
        if (i != 0 && i != 2) {
            assert(out[i] == initial[i]);
        }
    }
}

static unsigned long run_vstart(uint8_t *out, const uint8_t *initial,
                                const uint8_t *data, const uint8_t *mask,
                                unsigned long start)
{
    unsigned long final_vstart;

    asm volatile(
        "li t0, 16\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vle8.v v10, (%[initial])\n\t"
        "li t0, 4\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vle8.v v9, (%[data])\n\t"
        "vle8.v v8, (%[mask])\n\t"
        "csrw vstart, %[start]\n\t"
        ".word 0xca940557\n\t"
        "csrr %[final_vstart], vstart\n\t"
        "li t0, 16\n\t"
        "vsetvli zero, t0, e8, m1, tu, mu\n\t"
        "vse8.v v10, (%[out])"
        : [final_vstart] "=&r"(final_vstart)
        : [out] "r"(out), [initial] "r"(initial), [data] "r"(data),
          [mask] "r"(mask), [start] "r"(start)
        : "t0", "memory");
    return final_vstart;
}

static void test_vstart(void)
{
    static const uint8_t initial[16] __attribute__((aligned(16))) = {
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f
    };
    static const uint8_t data[16] __attribute__((aligned(16))) = {
        0xaa, 0xf0, 0x81, 0x55
    };
    static const uint8_t mask[16] __attribute__((aligned(16))) = {
        0xcc, 0x0f, 0x81, 0xaa
    };
    uint8_t out[16] __attribute__((aligned(16)));

    assert(run_vstart(out, initial, data, mask, 2) == 0);
    for (size_t i = 0; i < ARRAY_SIZE(out); i++) {
        if (i == 2 || i == 3) {
            assert(out[i] == ref_compress(data[i], mask[i]));
        } else {
            assert(out[i] == initial[i]);
        }
    }

    assert(run_vstart(out, initial, data, mask, 4) == 0);
    for (size_t i = 0; i < ARRAY_SIZE(out); i++) {
        assert(out[i] == initial[i]);
    }
}

int main(void)
{
    test_spec_example();
    test_widths();
    test_mask_and_tail();
    test_vstart();
    return 0;
}
