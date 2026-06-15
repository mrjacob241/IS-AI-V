#include "Vcpu.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void tick(Vcpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

static bool write_ppm(const char* path, const std::vector<uint32_t>& pixels) {
    FILE* f = fopen(path, "wb");
    if (!f)
        return false;

    fprintf(f, "P6\n800 480\n255\n");
    for (uint32_t rgb : pixels) {
        fputc((rgb >> 16) & 0xFF, f);
        fputc((rgb >> 8) & 0xFF, f);
        fputc(rgb & 0xFF, f);
    }

    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    const char* out_path = argc > 1 ? argv[1] : "screen.ppm";

    Vcpu top;
    top.clk = 0;
    top.rst = 1;
    top.dbg_ram_we = 0;
    top.dbg_ram_addr = 0;
    top.dbg_ram_wd = 0;
    top.dbg_rf_we = 0;
    top.dbg_rf_rd = 0;
    top.dbg_rf_wd = 0;
    top.eval();

    while (top.dbg_rom_loading)
        tick(top);

    top.dbg_rf_we = 1;
    top.dbg_rf_rd = 2;
    top.dbg_rf_wd = 0x0001FFF0;
    tick(top);
    top.dbg_rf_we = 0;

    top.rst = 0;
    top.eval();

    bool halted = false;
    for (int cycle = 0; cycle < 100000; cycle++) {
        if ((uint32_t)top.dbg_instr == 0x00000073u) {
            halted = true;
            break;
        }
        tick(top);
    }

    if (!halted) {
        printf("FAIL: timeout waiting for ecall\n");
        return 1;
    }

    do {
        tick(top);
    } while (!(top.video_x == 0 && top.video_y == 0));

    std::vector<uint32_t> pixels(800 * 480, 0);
    int captured = 0;
    while (captured < 800 * 480) {
        if (top.video_de) {
            uint32_t x = (uint32_t)top.video_x;
            uint32_t y = (uint32_t)top.video_y;
            if (x < 800 && y < 480) {
                pixels[y * 800 + x] = (uint32_t)top.video_rgb;
                captured++;
            }
        }
        tick(top);
    }

    if (!write_ppm(out_path, pixels)) {
        printf("FAIL: could not write %s\n", out_path);
        return 1;
    }

    printf("Wrote %s from virtual 800x480 video output\n", out_path);
    return 0;
}
