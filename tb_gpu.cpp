#include "Vgpu.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>

static int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-28s got=0x%08X expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while (0)

static void tick(Vgpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

static void reset_scan(Vgpu& top) {
    top.rst = 1;
    top.mmio_we = 0;
    tick(top);
    top.rst = 0;
    top.eval();
}

static void write_pixel(Vgpu& top, uint32_t index, uint32_t rgb) {
    top.mmio_addr = 0x00020000u + index * 4;
    top.mmio_wd = rgb;
    top.mmio_size = 2;
    top.mmio_we = 1;
    tick(top);
    top.mmio_we = 0;
    top.eval();
}

int main() {
    Vgpu top;
    top.clk = 0;
    top.rst = 0;
    top.mmio_we = 0;
    top.mmio_addr = 0;
    top.mmio_wd = 0;
    top.mmio_size = 2;

    write_pixel(top, 0, 0xFF0000);
    write_pixel(top, 1, 0x00FF00);
    write_pixel(top, 160, 0x0000FF);

    reset_scan(top);

    tick(top);
    ASSERT("x0", top.video_x, 0);
    ASSERT("y0", top.video_y, 0);
    ASSERT("pixel 0,0 red", top.video_rgb, 0xFF0000);

    for (int i = 1; i < 5; i++)
        tick(top);
    ASSERT("x4 still red", top.video_x, 4);
    ASSERT("pixel 4,0 red", top.video_rgb, 0xFF0000);

    tick(top);
    ASSERT("x5", top.video_x, 5);
    ASSERT("pixel 5,0 green", top.video_rgb, 0x00FF00);

    while (!(top.video_x == 0 && top.video_y == 4))
        tick(top);
    ASSERT("row 4 blue", top.video_rgb, 0x0000FF);

    while (!top.video_frame_done)
        tick(top);

    top.mmio_addr = 0x00040010;
    top.eval();
    ASSERT("frame register", top.mmio_rd, 1);

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
