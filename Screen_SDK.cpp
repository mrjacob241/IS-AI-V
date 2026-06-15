#include "Screen_SDK.h"

#ifndef __riscv
#include "Vcpu.h"
#include "verilated.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#endif

extern "C" {

static volatile unsigned int* const screen_fb =
    (volatile unsigned int*)0x00020000;
static volatile unsigned int* const screen_game_frame =
    (volatile unsigned int*)0x00040014;

static int Screen_Max(int a, int b) {
    return a > b ? a : b;
}

static int Screen_Min(int a, int b) {
    return a < b ? a : b;
}

void Screen_Draw_Rectangle(int x, int y, int w, int h, unsigned int color) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        int row = (py << 7) + (py << 5);
        for (int xx = 0; xx < w; xx++)
            screen_fb[row + x + xx] = color;
    }
}

void Screen_Clear(unsigned int color) {
    for (int y = 0; y < 120; y++) {
        int row = (y << 7) + (y << 5);
        for (int x = 0; x < 160; x++)
            screen_fb[row + x] = color;
    }
}

void Screen_Erase_Moved_Rectangle(int old_x, int old_y, int new_x, int new_y,
                                  int w, int h) {
    int old_r = old_x + w;
    int old_b = old_y + h;
    int new_r = new_x + w;
    int new_b = new_y + h;
    int ix0 = Screen_Max(old_x, new_x);
    int iy0 = Screen_Max(old_y, new_y);
    int ix1 = Screen_Min(old_r, new_r);
    int iy1 = Screen_Min(old_b, new_b);

    if (ix0 >= ix1 || iy0 >= iy1) {
        Screen_Draw_Rectangle(old_x, old_y, w, h, SCREEN_COLOR_BLACK);
        return;
    }

    if (old_y < iy0)
        Screen_Draw_Rectangle(old_x, old_y, w, iy0 - old_y, SCREEN_COLOR_BLACK);
    if (iy1 < old_b)
        Screen_Draw_Rectangle(old_x, iy1, w, old_b - iy1, SCREEN_COLOR_BLACK);
    if (old_x < ix0)
        Screen_Draw_Rectangle(old_x, iy0, ix0 - old_x, iy1 - iy0, SCREEN_COLOR_BLACK);
    if (ix1 < old_r)
        Screen_Draw_Rectangle(ix1, iy0, old_r - ix1, iy1 - iy0, SCREEN_COLOR_BLACK);
}

void Screen_Update(unsigned int game_frame) {
    *screen_game_frame = game_frame;
}

void Screen_Delay(int cycles) {
    for (volatile int i = 0; i < cycles; i++) {
    }
}

}

#ifndef __riscv
namespace ScreenSDK {

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_sigint(int) {
    stop_requested = 1;
}

void tick(Vcpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

bool write_ppm_atomic(const char* path, const std::vector<uint32_t>& pixels) {
    std::string tmp = std::string(path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f)
        return false;

    fprintf(f, "P6\n800 480\n255\n");
    for (uint32_t rgb : pixels) {
        fputc((rgb >> 16) & 0xFF, f);
        fputc((rgb >> 8) & 0xFF, f);
        fputc(rgb & 0xFF, f);
    }

    fclose(f);
    return rename(tmp.c_str(), path) == 0;
}

void init_cpu_inputs(Vcpu& top) {
    top.clk = 0;
    top.rst = 1;
    top.dbg_ram_we = 0;
    top.dbg_ram_addr = 0;
    top.dbg_ram_wd = 0;
    top.dbg_rf_we = 0;
    top.dbg_rf_rd = 0;
    top.dbg_rf_wd = 0;
    top.eval();
}

void boot_rom_and_release_reset(Vcpu& top) {
    while (top.dbg_rom_loading)
        tick(top);

    top.dbg_rf_we = 1;
    top.dbg_rf_rd = 2;
    top.dbg_rf_wd = 0x0001FFF0;
    tick(top);
    top.dbg_rf_we = 0;

    top.rst = 0;
    top.eval();
}

void wait_for_frame_start(Vcpu& top, bool& halted) {
    do {
        if (!halted && (uint32_t)top.dbg_instr == 0x00000073u)
            halted = true;
        tick(top);
    } while (!stop_requested && !(top.video_x == 0 && top.video_y == 0));
}

void capture_frame(Vcpu& top, std::vector<uint32_t>& pixels, bool& halted) {
    int captured = 0;
    while (!stop_requested && captured < 800 * 480) {
        if (!halted && (uint32_t)top.dbg_instr == 0x00000073u)
            halted = true;

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
}

void print_frame_status(int frame, const LiveConfig& config, uint32_t game_frame,
                        uint32_t last_game_frame, bool halted) {
    uint32_t game_delta = game_frame - last_game_frame;

    if (config.frames_to_write == 0) {
        printf("gfx_frame=%d game_frame=%u game_delta=%u -> %s%s\n",
               frame + 1, game_frame, game_delta, config.out_path,
               halted ? " (program halted)" : "");
    } else {
        printf("gfx_frame=%d/%d game_frame=%u game_delta=%u -> %s%s\n",
               frame + 1, config.frames_to_write, game_frame, game_delta,
               config.out_path, halted ? " (program halted)" : "");
    }
    fflush(stdout);
}

} // namespace

int env_int_allow_zero(const char* name, int fallback) {
    const char* value = getenv(name);
    if (!value)
        return fallback;
    int parsed = atoi(value);
    return parsed >= 0 ? parsed : fallback;
}

int run_live_capture(const LiveConfig& config) {
    LiveConfig cfg = config;
    if (!cfg.out_path)
        cfg.out_path = "screen.ppm";
    if (cfg.fps <= 0)
        cfg.fps = 30;

    stop_requested = 0;
    std::signal(SIGINT, handle_sigint);

    Vcpu top;
    init_cpu_inputs(top);
    boot_rom_and_release_reset(top);

    std::vector<uint32_t> pixels(800 * 480, 0);
    bool halted = false;
    uint32_t last_game_frame = 0;
    auto next_frame_time = std::chrono::steady_clock::now();
    auto frame_period = std::chrono::microseconds(1000000 / cfg.fps);

    for (int frame = 0;
         !stop_requested && (cfg.frames_to_write == 0 || frame < cfg.frames_to_write);
         frame++) {
        wait_for_frame_start(top, halted);
        capture_frame(top, pixels, halted);

        if (stop_requested)
            break;

        if (!write_ppm_atomic(cfg.out_path, pixels)) {
            printf("FAIL: could not write %s\n", cfg.out_path);
            return 1;
        }

        uint32_t game_frame = (uint32_t)top.video_game_frame;
        print_frame_status(frame, cfg, game_frame, last_game_frame, halted);
        last_game_frame = game_frame;

        next_frame_time += frame_period;
        std::this_thread::sleep_until(next_frame_time);
    }

    printf("stopped\n");
    return 0;
}

} // namespace ScreenSDK
#else
namespace ScreenSDK {
int env_int_allow_zero(const char*, int fallback) {
    return fallback;
}

int run_live_capture(const LiveConfig&) {
    return 1;
}
} // namespace ScreenSDK
#endif
