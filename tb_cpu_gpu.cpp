#include "Vcpu.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>

static int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-30s got=0x%08X expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while (0)

static void eval(Vcpu& top) {
    top.eval();
}

static void tick(Vcpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

static void init_inputs(Vcpu& top) {
    top.clk = 0;
    top.rst = 1;
    top.dbg_ram_we = 0;
    top.dbg_ram_addr = 0;
    top.dbg_ram_wd = 0;
    top.dbg_rf_we = 0;
    top.dbg_rf_rd = 0;
    top.dbg_rf_wd = 0;
}

static void store_instr(Vcpu& top, uint32_t addr, uint32_t instr) {
    top.dbg_ram_addr = addr;
    top.dbg_ram_wd = instr;
    top.dbg_ram_we = 1;
    tick(top);
    top.dbg_ram_we = 0;
    eval(top);
}

static void load_program(Vcpu& top, const uint32_t* program, size_t words) {
    top.rst = 1;
    eval(top);
    for (size_t i = 0; i < words; i++)
        store_instr(top, (uint32_t)(i * 4), program[i]);
    top.rst = 0;
    eval(top);
}

int main() {
    Vcpu top;
    init_inputs(top);

    const uint32_t program[] = {
        0x000200B7, // lui  x1, 0x20       ; x1 = 0x00020000
        0x00FF0137, // lui  x2, 0x00FF0    ; x2 = 0x00FF0000
        0x0020A023, // sw   x2, 0(x1)      ; fb[0] = red
        0x0000F137, // lui  x2, 0xF        ; x2 = 0x0000F000
        0x0020A223, // sw   x2, 4(x1)      ; fb[1] = green
        0x0FF00113, // addi x2, x0, 255    ; x2 = 0x000000FF
        0x2820A023, // sw   x2, 640(x1)    ; fb[160] = blue
    };

    load_program(top, program, sizeof(program) / sizeof(program[0]));
    for (int i = 0; i < 8; i++)
        tick(top);

    tick(top);
    while (!(top.video_x == 0 && top.video_y == 0))
        tick(top);

    ASSERT("cpu video x0", top.video_x, 0);
    ASSERT("cpu video y0", top.video_y, 0);
    ASSERT("cpu video red", top.video_rgb, 0xFF0000);

    for (int i = 1; i < 5; i++)
        tick(top);
    ASSERT("cpu x4 red", top.video_x, 4);
    ASSERT("cpu video red repeat", top.video_rgb, 0xFF0000);

    tick(top);
    ASSERT("cpu x5 green", top.video_x, 5);
    ASSERT("cpu video green", top.video_rgb, 0x00F000);

    while (!(top.video_x == 0 && top.video_y == 4))
        tick(top);
    ASSERT("cpu video blue row", top.video_rgb, 0x0000FF);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
