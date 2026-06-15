#include "Vram.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>

int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-45s  got=0x%08X  expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

void tick(Vram& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}

// Write via data port, then de-assert we
void store(Vram& top, uint32_t addr, uint32_t data, uint8_t size) {
    top.daddr = addr;
    top.wd    = data;
    top.size  = size;
    top.we    = 1;
    tick(top);
    top.we = 0;
}

// Read via data port (combinational)
uint32_t load(Vram& top, uint32_t addr, uint8_t size, uint8_t uns) {
    top.daddr        = addr;
    top.size         = size;
    top.is_unsigned  = uns;
    top.eval();
    return (uint32_t)top.rd;
}

int main() {
    Vram top;
    top.clk         = 0;
    top.we          = 0;
    top.daddr       = 0;
    top.iaddr       = 0;
    top.wd          = 0;
    top.size        = 2;
    top.is_unsigned = 0;
    top.eval();

    // ----------------------------------------------------------------
    // RAM-01 — word store and load
    // ----------------------------------------------------------------
    store(top, 0x100, 0xDEADBEEF, 2);
    ASSERT("RAM-01 SW then LW", load(top, 0x100, 2, 0), 0xDEADBEEF);

    // ----------------------------------------------------------------
    // RAM-02 — byte load signed (LB)
    // little-endian: byte 0 of 0xDEADBEEF = 0xEF
    // ----------------------------------------------------------------
    ASSERT("RAM-02 LB  sign-ext 0xEF", load(top, 0x100, 0, 0), 0xFFFFFFEF);

    // ----------------------------------------------------------------
    // RAM-03 — byte load unsigned (LBU)
    // ----------------------------------------------------------------
    ASSERT("RAM-03 LBU zero-ext 0xEF", load(top, 0x100, 0, 1), 0x000000EF);

    // ----------------------------------------------------------------
    // RAM-04 — halfword load signed (LH)
    // halfword 0 = {mem[0x101], mem[0x100]} = {0xBE, 0xEF} = 0xBEEF
    // ----------------------------------------------------------------
    ASSERT("RAM-04 LH  sign-ext 0xBEEF", load(top, 0x100, 1, 0), 0xFFFFBEEF);

    // ----------------------------------------------------------------
    // RAM-05 — halfword load unsigned (LHU)
    // ----------------------------------------------------------------
    ASSERT("RAM-05 LHU zero-ext 0xBEEF", load(top, 0x100, 1, 1), 0x0000BEEF);

    // ----------------------------------------------------------------
    // RAM-06 — byte store does not disturb adjacent bytes
    // Write 0xAABBCCDD at 0x200, then overwrite byte at 0x201 with 0xFF
    // ----------------------------------------------------------------
    store(top, 0x200, 0xAABBCCDD, 2);
    store(top, 0x201, 0xFF,       0);   // SB: only byte at 0x201 changes
    ASSERT("RAM-06 SB no side-effects", load(top, 0x200, 2, 0), 0xAABBFFDD);

    // ----------------------------------------------------------------
    // RAM-07 — halfword store (SH)
    // ----------------------------------------------------------------
    store(top, 0x204, 0x1234, 1);
    ASSERT("RAM-07 SH then LHU", load(top, 0x204, 1, 1), 0x00001234);

    // ----------------------------------------------------------------
    // RAM-08 — write enable gating
    // Attempt a write with we=0, data at 0x100 must stay as 0xDEADBEEF
    // ----------------------------------------------------------------
    top.we = 0; top.daddr = 0x100; top.wd = 0x12345678; top.size = 2;
    tick(top);
    ASSERT("RAM-08 WE gating", load(top, 0x100, 2, 0), 0xDEADBEEF);

    // ----------------------------------------------------------------
    // RAM-09 — multiple word addresses independent
    // ----------------------------------------------------------------
    store(top, 0x300, 0x11111111, 2);
    store(top, 0x304, 0x22222222, 2);
    store(top, 0x308, 0x33333333, 2);
    ASSERT("RAM-09 addr 0x300", load(top, 0x300, 2, 0), 0x11111111);
    ASSERT("RAM-09 addr 0x304", load(top, 0x304, 2, 0), 0x22222222);
    ASSERT("RAM-09 addr 0x308", load(top, 0x308, 2, 0), 0x33333333);

    // ----------------------------------------------------------------
    // RAM-10 — instruction port reads same backing array as data port
    // ----------------------------------------------------------------
    store(top, 0x400, 0xDEADBEEF, 2);
    top.iaddr = 0x400;
    top.eval();
    ASSERT("RAM-10 iport reads data written via dport", (uint32_t)top.instr, 0xDEADBEEF);

    // ----------------------------------------------------------------
    // RAM-11 — instruction port is combinational (no clock needed)
    // ----------------------------------------------------------------
    store(top, 0x404, 0xCAFEBABE, 2);
    top.iaddr = 0x400; top.eval();
    uint32_t v0 = top.instr;
    top.iaddr = 0x404; top.eval();
    uint32_t v1 = top.instr;
    ASSERT("RAM-11 iport combinational addr=0x400", v0, 0xDEADBEEF);
    ASSERT("RAM-11 iport combinational addr=0x404", v1, 0xCAFEBABE);

    // ----------------------------------------------------------------
    // RAM-12 — code region and stack region coexist
    // ----------------------------------------------------------------
    store(top, 0x00000000, 0xAAAAAAAA, 2);
    store(top, 0x0001FFF0, 0xBBBBBBBB, 2);
    ASSERT("RAM-12 code region  0x00000000", load(top, 0x00000000, 2, 0), 0xAAAAAAAA);
    ASSERT("RAM-12 stack region 0x0001FFF0", load(top, 0x0001FFF0, 2, 0), 0xBBBBBBBB);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
