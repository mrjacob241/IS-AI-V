#include "Valu.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-38s  got=0x%08X  expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

// ALU op codes
static const uint8_t ADD  = 0x0;
static const uint8_t SUB  = 0x1;
static const uint8_t AND  = 0x2;
static const uint8_t OR   = 0x3;
static const uint8_t XOR  = 0x4;
static const uint8_t SLL  = 0x5;
static const uint8_t SRL  = 0x6;
static const uint8_t SRA  = 0x7;
static const uint8_t SLT  = 0x8;
static const uint8_t SLTU = 0x9;

void check(Valu& top, const char* name,
           uint32_t a, uint32_t b, uint8_t op,
           uint32_t exp_y, uint8_t exp_zero) {
    top.a = a; top.b = b; top.op = op;
    top.eval();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s (y)", name);
    ASSERT(buf, top.y, exp_y);
    snprintf(buf, sizeof(buf), "%s (zero)", name);
    ASSERT(buf, (uint32_t)top.zero, (uint32_t)exp_zero);
}

int main() {
    Valu top;

    // ALU-01: ADD basic
    check(top, "ALU-01 ADD 5+3",        5, 3, ADD, 8, 0);

    // ALU-02: ADD overflow wraps
    check(top, "ALU-02 ADD overflow",   0xFFFFFFFF, 1, ADD, 0x00000000, 1);

    // ALU-03: SUB basic
    check(top, "ALU-03 SUB 10-3",       10, 3, SUB, 7, 0);

    // ALU-04: SUB same operands → zero flag
    check(top, "ALU-04 SUB zero flag",  5, 5, SUB, 0, 1);

    // ALU-05: AND
    check(top, "ALU-05 AND",            0xFF00FF00, 0xF0F0F0F0, AND, 0xF000F000, 0);

    // ALU-06: OR
    check(top, "ALU-06 OR",             0xFF000000, 0x00FF0000, OR,  0xFFFF0000, 0);

    // ALU-07: XOR
    check(top, "ALU-07 XOR",            0xFFFFFFFF, 0x0F0F0F0F, XOR, 0xF0F0F0F0, 0);

    // ALU-08: SLL
    check(top, "ALU-08 SLL 1<<4",       1, 4,  SLL, 16,         0);
    check(top, "ALU-08 SLL 1<<31",      1, 31, SLL, 0x80000000, 0);

    // ALU-09: SRL (fills with 0)
    check(top, "ALU-09 SRL 0x80000000>>1",  0x80000000, 1, SRL, 0x40000000, 0);
    check(top, "ALU-09 SRL 0xFFFFFFFF>>4",  0xFFFFFFFF, 4, SRL, 0x0FFFFFFF, 0);

    // ALU-10: SRA (fills with sign bit)
    check(top, "ALU-10 SRA 0x80000000>>1",  0x80000000, 1, SRA, 0xC0000000, 0);
    check(top, "ALU-10 SRA 0xFFFFFFFF>>4",  0xFFFFFFFF, 4, SRA, 0xFFFFFFFF, 0);

    // ALU-11: SLT signed
    check(top, "ALU-11 SLT 3<5",        3, 5, SLT, 1, 0);
    check(top, "ALU-11 SLT 5<3",        5, 3, SLT, 0, 1);
    check(top, "ALU-11 SLT -1<0",       0xFFFFFFFF, 0, SLT, 1, 0); // -1 < 0 signed

    // ALU-12: SLTU unsigned
    check(top, "ALU-12 SLTU 0xFFFFFFFF<0",  0xFFFFFFFF, 0,          SLTU, 0, 1);
    check(top, "ALU-12 SLTU 0<0xFFFFFFFF",  0,          0xFFFFFFFF, SLTU, 1, 0);

    // ALU-13: shift uses only b[4:0]
    check(top, "ALU-13 SLL b[4:0]=1",   1, 0xFFFFFFE1, SLL, 2, 0);

    // ALU-14: zero flag independent checks
    top.a = 7; top.b = 7; top.op = SUB; top.eval();
    ASSERT("ALU-14 zero=1 when y==0",   (uint32_t)top.zero, 1u);
    top.a = 7; top.b = 6; top.op = SUB; top.eval();
    ASSERT("ALU-14 zero=0 when y!=0",   (uint32_t)top.zero, 0u);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
