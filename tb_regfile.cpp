#include "Vregfile.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>

int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-35s  got=0x%08X  expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

void tick(Vregfile& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}

void write_reg(Vregfile& top, uint8_t rd, uint32_t val) {
    top.rd = rd;
    top.wd = val;
    top.we = 1;
    tick(top);
    top.we = 0;
}

int main() {
    Vregfile top;
    top.clk = 0;
    top.we  = 0;
    top.rs1 = 0;
    top.rs2 = 0;
    top.rd  = 0;
    top.wd  = 0;
    top.eval();

    // RF-01: basic write then read
    write_reg(top, 1, 0xDEADBEEF);
    top.rs1 = 1;
    top.eval();
    ASSERT("RF-01 basic write/read", top.a, 0xDEADBEEF);

    // RF-02: x0 always zero — even after a write attempt
    write_reg(top, 0, 0xDEADBEEF);
    top.rs1 = 0;
    top.eval();
    ASSERT("RF-02 x0 always zero", top.a, 0x00000000);

    // RF-03: write enable gating — x2 never written with we=1
    top.rd = 2; top.wd = 0xCAFEBABE; top.we = 0;
    tick(top);
    top.rs1 = 2;
    top.eval();
    ASSERT("RF-03 write enable gating", top.a, 0x00000000);

    // RF-04: simultaneous two-port read
    write_reg(top, 1, 10);
    write_reg(top, 2, 3);
    top.rs1 = 1; top.rs2 = 2;
    top.eval();
    ASSERT("RF-04 two-port read (a)", top.a, 10);
    ASSERT("RF-04 two-port read (b)", top.b, 3);

    // RF-05: write to one register does not disturb another
    write_reg(top, 3, 100);
    write_reg(top, 4, 200);
    top.rs1 = 3; top.rs2 = 4;
    top.eval();
    ASSERT("RF-05 no cross-write (a)", top.a, 100);
    ASSERT("RF-05 no cross-write (b)", top.b, 200);

    // RF-06: all 31 writable registers are independent
    for (int i = 1; i <= 31; i++)
        write_reg(top, (uint8_t)i, (uint32_t)(i * 4));

    bool all_pass = true;
    for (int i = 1; i <= 31; i++) {
        top.rs1 = (uint8_t)i;
        top.eval();
        if ((uint32_t)top.a != (uint32_t)(i * 4)) {
            printf("FAIL: RF-06 x%-2d  got=0x%08X  expected=0x%08X\n",
                   i, (uint32_t)top.a, (uint32_t)(i * 4));
            failures++;
            all_pass = false;
        }
    }
    if (all_pass)
        printf("PASS: RF-06 all 31 registers independent\n");

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
