#include "Vcpu.h"
#include "Vcpu___024root.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

static void tick(Vcpu& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}

static uint32_t get_reg(Vcpu& top, int i) {
    return (uint32_t)top.rootp->cpu__DOT__rf__DOT__regs[i];
}

int main() {
    const char* name     = getenv("TEST_NAME");
    const char* exp_str  = getenv("EXPECTED_A0");
    bool        do_check = (exp_str != nullptr);
    uint32_t    expected = do_check ? (uint32_t)strtoul(exp_str, nullptr, 0) : 0;

    Vcpu top;
    top.clk        = 0;
    top.rst        = 1;
    top.dbg_ram_we = 0; top.dbg_ram_addr = 0; top.dbg_ram_wd = 0;
    top.dbg_rf_we  = 0; top.dbg_rf_rd   = 0; top.dbg_rf_wd  = 0;
    top.eval();

    // ROM loading — tick until rom_loading goes low (works for any program size)
    top.eval();
    while (top.dbg_rom_loading) tick(top);

    // Init sp = top of stack
    top.dbg_rf_we = 1; top.dbg_rf_rd = 2; top.dbg_rf_wd = 0x0001FFF0;
    tick(top);
    top.dbg_rf_we = 0;

    // Release reset
    top.rst = 0;
    top.eval();

    static const int MAX_CYCLES = 100000;
    bool halted = false;

    for (int cycle = 0; cycle < MAX_CYCLES; cycle++) {
        if ((uint32_t)top.dbg_instr == 0x00000073u) {
            halted = true;
            break;
        }
        tick(top);
    }

    const char* label = name ? name : "program";
    uint32_t actual = get_reg(top, 10);

    if (!halted) {
        printf("FAIL: %s — timeout after %d cycles\n", label, MAX_CYCLES);
        return 1;
    }

    if (do_check) {
        if (actual == expected) {
            printf("PASS: %s — a0=0x%08X (%u)\n", label, actual, actual);
            return 0;
        } else {
            printf("FAIL: %s — expected a0=0x%08X (%u), got 0x%08X (%u)\n",
                   label, expected, expected, actual, actual);
            return 1;
        }
    }

    printf("%s — a0=0x%08X (%u)  PC=0x%04X\n", label, actual, actual, (uint32_t)top.dbg_pc);
    return 0;
}
