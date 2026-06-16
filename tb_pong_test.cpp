#include "Vcpu.h"
#include "pong_input_io.h"
#include "verilated.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

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

int env_int_allow_zero(const char* name, int fallback) {
    const char* value = getenv(name);
    if (!value)
        return fallback;
    int parsed = atoi(value);
    return parsed >= 0 ? parsed : fallback;
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

void wait_for_frame_start(Vcpu& top, bool& halted, PongInputIO& input) {
    do {
        if (!halted && (uint32_t)top.dbg_instr == 0x00000073u)
            halted = true;
        input.apply_to_cpu(top);
        tick(top);
    } while (!stop_requested && !input.quit_requested() &&
             !(top.video_x == 0 && top.video_y == 0));
}

void capture_frame(Vcpu& top, std::vector<uint32_t>& pixels, bool& halted,
                   PongInputIO& input) {
    int captured = 0;
    while (!stop_requested && !input.quit_requested() && captured < 800 * 480) {
        if (!halted && (uint32_t)top.dbg_instr == 0x00000073u)
            halted = true;

        input.apply_to_cpu(top);

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

} // namespace

int main(int argc, char** argv) {
    const char* out_path = argc > 1 ? argv[1] : "screen.ppm";
    int frames_to_write = env_int_allow_zero("LIVE_FRAMES", 0);
    int fps = env_int_allow_zero("LIVE_FPS", 30);
    if (fps <= 0)
        fps = 30;

    stop_requested = 0;
    std::signal(SIGINT, handle_sigint);

    PongInputIO input;
    printf("Controls: W=up, S=down, Q=quit. Writing %s at %d FPS.\n",
           out_path, fps);
    if (!input.enabled())
        printf("stdin is not a terminal; keyboard controls are disabled.\n");
    fflush(stdout);

    Vcpu top;
    init_cpu_inputs(top);
    boot_rom_and_release_reset(top);

    std::vector<uint32_t> pixels(800 * 480, 0);
    bool halted = false;
    uint32_t last_game_frame = 0;
    auto next_frame_time = std::chrono::steady_clock::now();
    auto frame_period = std::chrono::microseconds(1000000 / fps);

    for (int frame = 0;
         !stop_requested && !input.quit_requested() &&
             (frames_to_write == 0 || frame < frames_to_write);
         frame++) {
        wait_for_frame_start(top, halted, input);
        capture_frame(top, pixels, halted, input);

        if (stop_requested || input.quit_requested())
            break;

        if (!write_ppm_atomic(out_path, pixels)) {
            printf("FAIL: could not write %s\n", out_path);
            return 1;
        }

        uint32_t game_frame = (uint32_t)top.video_game_frame;
        uint32_t game_delta = game_frame - last_game_frame;
        if (frames_to_write == 0) {
            printf("gfx_frame=%d game_frame=%u game_delta=%u -> %s%s\n",
                   frame + 1, game_frame, game_delta, out_path,
                   halted ? " (program halted)" : "");
        } else {
            printf("gfx_frame=%d/%d game_frame=%u game_delta=%u -> %s%s\n",
                   frame + 1, frames_to_write, game_frame, game_delta, out_path,
                   halted ? " (program halted)" : "");
        }
        fflush(stdout);
        last_game_frame = game_frame;

        next_frame_time += frame_period;
        std::this_thread::sleep_until(next_frame_time);
    }

    input.apply_to_cpu(top);
    printf("stopped\n");
    return 0;
}
