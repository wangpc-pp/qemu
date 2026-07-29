/*
 * Functional test for the Zilx indexed-load extension.
 *
 * Zilx encodes its loads on the AMO major opcode (0x2f) with aq=rl=0.
 * The assembler does not (yet) know the mnemonics, so each instruction is
 * emitted with a ".insn r" directive.  In that encoding rs1 is the index and
 * rs2 is the base, matching the Zilx operand roles, and func7 packs the mode:
 *
 *   func7 = (funct5 << 2) | (aq << 1) | rl
 *
 *   funct5 = 10010 (unscaled, "lx")           -> func7 = 0x48
 *   funct5 = 11010 (scaled,   "lxs")          -> func7 = 0x68
 *   funct5 = 11110 (scaled uw-index, "lxsuw") -> func7 = 0x78
 *
 * Effective address: EA = base + (index << log2(access-size)); for the
 * lxsuw* forms the index is first zero-extended from its low 32 bits.
 */

#include <stdint.h>
#include <stdio.h>

#define ZILX_LOAD(func3, func7, base, index)                            \
    ({                                                                  \
        long __rd;                                                      \
        asm volatile(".insn r 0x2f, %1, %2, %0, %3, %4"                 \
                     : "=r"(__rd)                                       \
                     : "i"(func3), "i"(func7),                          \
                       "r"((long)(index)), "r"((long)(base)));          \
        __rd;                                                           \
    })

/* Unscaled indexed loads (func7 = 0x48). */
#define lxh(base, index)   ZILX_LOAD(1, 0x48, base, index)
#define lxw(base, index)   ZILX_LOAD(2, 0x48, base, index)
#define lxd(base, index)   ZILX_LOAD(3, 0x48, base, index)
#define lxhu(base, index)  ZILX_LOAD(5, 0x48, base, index)
#define lxwu(base, index)  ZILX_LOAD(6, 0x48, base, index)

/* Scaled indexed loads (func7 = 0x68). */
#define lxsb(base, index)  ZILX_LOAD(0, 0x68, base, index)
#define lxsh(base, index)  ZILX_LOAD(1, 0x68, base, index)
#define lxsw(base, index)  ZILX_LOAD(2, 0x68, base, index)
#define lxsd(base, index)  ZILX_LOAD(3, 0x68, base, index)
#define lxsbu(base, index) ZILX_LOAD(4, 0x68, base, index)
#define lxshu(base, index) ZILX_LOAD(5, 0x68, base, index)
#define lxswu(base, index) ZILX_LOAD(6, 0x68, base, index)

/* Scaled loads with zero-extended 32-bit index (func7 = 0x78). */
#define lxsuwb(base, index)  ZILX_LOAD(0, 0x78, base, index)
#define lxsuwh(base, index)  ZILX_LOAD(1, 0x78, base, index)
#define lxsuww(base, index)  ZILX_LOAD(2, 0x78, base, index)
#define lxsuwd(base, index)  ZILX_LOAD(3, 0x78, base, index)
#define lxsuwbu(base, index) ZILX_LOAD(4, 0x78, base, index)
#define lxsuwhu(base, index) ZILX_LOAD(5, 0x78, base, index)
#define lxsuwwu(base, index) ZILX_LOAD(6, 0x78, base, index)

static int failures;

#define CHECK(expr, expected)                                           \
    do {                                                                \
        long __got = (long)(expr);                                      \
        long __exp = (long)(expected);                                  \
        if (__got != __exp) {                                           \
            printf("FAIL %s: got %#lx, expected %#lx\n",                \
                   #expr, __got, __exp);                                \
            failures++;                                                 \
        }                                                               \
    } while (0)

/*
 * Known data pattern.  buf[i] = 0x10 + i, so the little-endian views are:
 *   halfword at byte k = (0x11+k)<<8 | (0x10+k)
 *   word     at byte k = ...
 * We pick a byte value with the sign bit set (0x80+) to exercise sign vs
 * zero extension.
 */
static uint8_t buf[64] __attribute__((aligned(8)));

int main(void)
{
    for (int i = 0; i < (int)sizeof(buf); i++) {
        buf[i] = 0x80 + i; /* high bit set: 0x80, 0x81, 0x82, ... */
    }

    uintptr_t p = (uintptr_t)buf;

    /*
     * Unscaled: EA = base + index.  Read the halfword/word/doubleword that
     * starts at byte offset 4.
     */
    CHECK(lxh(p, 4), (int16_t)0x8584);
    CHECK(lxhu(p, 4), (uint16_t)0x8584);
    CHECK(lxw(p, 4), (int32_t)0x87868584);
    CHECK(lxwu(p, 4), (uint32_t)0x87868584u);
    CHECK(lxd(p, 4), (int64_t)0x8b8a898887868584ULL);

    /* Unscaled with base and index swapped still lands on the same address. */
    CHECK(lxw(p + 4, 0), (int32_t)0x87868584);
    CHECK(lxw(p, 4), lxw(p + 4, 0));

    /*
     * Scaled: EA = base + (index << log2(size)).  index=2 reaches byte 2/4/8/16
     * for byte/half/word/dword.
     */
    CHECK(lxsb(p, 5), (int8_t)0x85);
    CHECK(lxsbu(p, 5), (uint8_t)0x85);
    CHECK(lxsh(p, 2), (int16_t)0x8584);      /* byte offset 2*2 = 4 */
    CHECK(lxshu(p, 2), (uint16_t)0x8584);
    CHECK(lxsw(p, 1), (int32_t)0x87868584);  /* byte offset 1*4 = 4 */
    CHECK(lxswu(p, 1), (uint32_t)0x87868584u);
    CHECK(lxsd(p, 1), (int64_t)0x8f8e8d8c8b8a8988ULL); /* offset 1*8 = 8 */

    /*
     * Scaled with zero-extended 32-bit index.  A negative 64-bit index whose
     * low 32 bits are small must be treated as that small unsigned value.
     */
    long dirty_index = (long)0xffffffff00000001ULL; /* low32 = 1 */
    CHECK(lxsuwb(p, dirty_index), (int8_t)0x81);     /* offset 1 */
    CHECK(lxsuwbu(p, dirty_index), (uint8_t)0x81);
    CHECK(lxsuwh(p, dirty_index), (int16_t)0x8382);  /* offset 1<<1 = 2 */
    CHECK(lxsuwhu(p, dirty_index), (uint16_t)0x8382);
    CHECK(lxsuww(p, dirty_index), (int32_t)0x87868584);   /* offset 1<<2 = 4 */
    CHECK(lxsuwwu(p, dirty_index), (uint32_t)0x87868584u);
    CHECK(lxsuwd(p, dirty_index), (int64_t)0x8f8e8d8c8b8a8988ULL); /* 1<<3=8 */

    /* Plain small index behaves like the non-uw scaled forms. */
    CHECK(lxsuww(p, 1), lxsw(p, 1));

    if (failures == 0) {
        printf("Zilx: all checks passed\n");
        return 0;
    }
    printf("Zilx: %d checks failed\n", failures);
    return 1;
}
