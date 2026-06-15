#include "Vimm_gen.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>

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

void check(Vimm_gen& top, const char* name, uint32_t instr, uint32_t exp) {
    top.instr = instr;
    top.eval();
    ASSERT(name, top.imm, exp);
}

int main() {
    Vimm_gen top;

    // --- I-type ALU ---
    // addi x1, x0, 5  →  imm = 5
    check(top, "IMM-01 I-type positive (addi +5)",      0x00500093, 0x00000005);

    // addi x1, x0, -1  →  imm = 0xFFFFFFFF
    check(top, "IMM-02 I-type negative sign-ext (addi -1)", 0xFFF00093, 0xFFFFFFFF);

    // --- I-type load ---
    // lw x1, 4(x2)  →  imm = 4
    check(top, "IMM-03 I-type load (lw +4)",             0x00412083, 0x00000004);

    // --- I-type JALR ---
    // jalr x1, 0(x5)  →  imm = 0
    check(top, "IMM-04 I-type JALR (imm=0)",             0x000280E7, 0x00000000);

    // --- S-type ---
    // sw x1, 12(x2)  →  imm = 12
    check(top, "IMM-05 S-type positive (sw +12)",        0x00112623, 0x0000000C);

    // sw x1, -4(x2)  →  imm = 0xFFFFFFFC
    check(top, "IMM-06 S-type negative (sw -4)",         0xFE112E23, 0xFFFFFFFC);

    // --- B-type ---
    // beq x1, x2, +8  →  imm = 8
    check(top, "IMM-07 B-type positive (beq +8)",        0x00208463, 0x00000008);

    // beq x1, x2, -8  →  imm = 0xFFFFFFF8
    check(top, "IMM-08 B-type negative (beq -8)",        0xFE208CE3, 0xFFFFFFF8);

    // --- U-type ---
    // lui x1, 0x12345  →  imm = 0x12345000
    check(top, "IMM-09 U-type LUI",                      0x123452B7, 0x12345000);

    // auipc x1, 1  →  imm = 0x00001000
    check(top, "IMM-10 U-type AUIPC",                    0x00001097, 0x00001000);

    // --- J-type ---
    // jal x1, +12  →  imm = 12
    check(top, "IMM-11 J-type positive (jal +12)",       0x00C000EF, 0x0000000C);

    // jal x0, -4  →  imm = 0xFFFFFFFC
    check(top, "IMM-12 J-type negative (jal -4)",        0xFFDFF06F, 0xFFFFFFFC);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
