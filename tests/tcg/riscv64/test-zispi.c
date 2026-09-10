/*
 * Functional test for the experimental Zispi extension.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define ZISPI_STORE(funct3, base, value)                                \
    asm volatile(".insn r 0x2f, %2, 0x58, %0, %0, %1"                 \
                 : "+r"(base)                                         \
                 : "r"((long)(value)), "i"(funct3)                  \
                 : "memory")

static int failures;

#define CHECK(got, expected)                                            \
    do {                                                                \
        unsigned long __got = (unsigned long)(got);                     \
        unsigned long __expected = (unsigned long)(expected);           \
        if (__got != __expected) {                                      \
            printf("FAIL %s: got 0x%lx, expected 0x%lx\n",            \
                   #got, __got, __expected);                            \
            failures++;                                                 \
        }                                                               \
    } while (0)

static void reserved_rd_mismatch(void)
{
    asm volatile(".insn r 0x2f, 0, 0x58, a0, a1, a2");
}

static void reserved_x0_base(void)
{
    asm volatile(".insn r 0x2f, 0, 0x58, zero, zero, a0");
}

static void check_reserved(void (*fn)(void), const char *name)
{
    pid_t pid = fork();
    int status;

    if (pid == 0) {
        fn();
        _exit(0);
    }
    if (pid < 0 || waitpid(pid, &status, 0) != pid ||
        !WIFSIGNALED(status) || WTERMSIG(status) != SIGILL) {
        printf("FAIL %s did not raise SIGILL\n", name);
        failures++;
    }
}

int main(void)
{
    uint8_t buf[32] __attribute__((aligned(8))) = { 0 };
    uintptr_t old_p;
    uintptr_t p;

    p = (uintptr_t)&buf[0];
    ZISPI_STORE(0, p, 0x81);
    CHECK(p, (uintptr_t)&buf[1]);
    CHECK(buf[0], 0x81);

    p = (uintptr_t)&buf[2];
    ZISPI_STORE(1, p, 0x8382);
    CHECK(p, (uintptr_t)&buf[4]);
    CHECK(*(uint16_t *)&buf[2], 0x8382);

    p = (uintptr_t)&buf[4];
    ZISPI_STORE(2, p, 0x87868584);
    CHECK(p, (uintptr_t)&buf[8]);
    CHECK(*(uint32_t *)&buf[4], 0x87868584);

    p = (uintptr_t)&buf[8];
    ZISPI_STORE(3, p, 0x8f8e8d8c8b8a8988ULL);
    CHECK(p, (uintptr_t)&buf[16]);
    CHECK(*(uint64_t *)&buf[8], 0x8f8e8d8c8b8a8988ULL);

    p = (uintptr_t)&buf[16];
    old_p = p;
    asm volatile(".insn r 0x2f, 3, 0x58, %0, %0, %0"
                 : "+r"(p)
                 :
                 : "memory");
    CHECK(p, old_p + 8);
    CHECK(*(uintptr_t *)&buf[16], old_p);

    check_reserved(reserved_rd_mismatch, "rd != rs1");
    check_reserved(reserved_x0_base, "rs1 = x0");

    if (failures == 0) {
        puts("Zispi: all checks passed");
        return 0;
    }
    printf("Zispi: %d checks failed\n", failures);
    return 1;
}
