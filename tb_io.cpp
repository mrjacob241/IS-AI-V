#include "Vcpu.h"
#include "Vcpu___024root.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>

static void tick(Vcpu& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}

static uint32_t get_reg(Vcpu& top, int i) {
    return (uint32_t)top.rootp->cpu__DOT__rf__DOT__regs[i];
}

// Syscall table (a7 value → action):
//   1   print signed int  (a0 = value)
//   11  print char        (a0 = ASCII code)
//   12  read char         (return in a0)
//   93  exit

int main() {
    Vcpu top;
    top.clk        = 0;
    top.rst        = 1;
    top.dbg_ram_we = 0; top.dbg_ram_addr = 0; top.dbg_ram_wd = 0;
    top.dbg_rf_we  = 0; top.dbg_rf_rd   = 0; top.dbg_rf_wd  = 0;
    top.eval();

    top.eval();
    while (top.dbg_rom_loading) tick(top);

    top.dbg_rf_we = 1; top.dbg_rf_rd = 2; top.dbg_rf_wd = 0x0001FFF0;
    tick(top);
    top.dbg_rf_we = 0;

    top.rst = 0;
    top.eval();

    for (int cycle = 0; cycle < 100000000; cycle++) {
        if ((uint32_t)top.dbg_instr != 0x00000073u) {
            tick(top);
            continue;
        }

        uint32_t a7 = get_reg(top, 17);
        uint32_t a0 = get_reg(top, 10);

        if (a7 == 93) {
            break;

        } else if (a7 == 1) {
            printf("%d", (int32_t)a0);
            fflush(stdout);
            tick(top);

        } else if (a7 == 11) {
            putchar((int)(a0 & 0xFF));
            fflush(stdout);
            tick(top);

        } else if (a7 == 12) {
            int ch = getchar();
            top.dbg_rf_we = 1; top.dbg_rf_rd = 10; top.dbg_rf_wd = (uint32_t)ch;
            tick(top);
            top.dbg_rf_we = 0;

        } else {
            tick(top);
        }
    }

    return 0;
}
