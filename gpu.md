# gpu.v

`gpu.v` implements a tiny memory-mapped raster output block for simulation.
Software writes a low-resolution framebuffer, and the GPU scans it out as an
800x480 virtual HDMI-style connector.

For the full rendering pipeline, host-side C++ capture, Python live viewer, and
Pong game rendering notes, see `GPU_RENDERING.md`.

## Memory Map

| Address range | Meaning |
|---------------|---------|
| `0x00020000` - `0x00032BFF` | 160x120 framebuffer, one 32-bit word per pixel |
| `0x00040000` - `0x000400FF` | GPU registers |

Framebuffer words use `0x00RRGGBB`. The first implementation supports word
stores for pixels.

GPU registers:

| Offset | Value |
|--------|-------|
| `0x00` | Source framebuffer width: 160 |
| `0x04` | Source framebuffer height: 120 |
| `0x08` | Output width: 800 |
| `0x0C` | Output height: 480 |
| `0x10` | Frame counter |
| `0x14` | Read/write software debug counter, used by `pong_live` as the game-frame counter |

## Scanout

The GPU performs nearest-neighbor hardware upscaling:

```text
160x120 source -> 800x480 output
horizontal scale: 5x
vertical scale:   4x
```

The exported virtual connector signals are:

```verilog
video_rgb
video_hsync
video_vsync
video_de
video_x
video_y
video_frame_done
```

This is not TMDS HDMI. It is the pre-encoder RGB/sync/data-enable stream a C++
Verilator testbench can capture or display.

## CPU Integration

`cpu.v` routes normal load/store instructions to the GPU when the ALU address
falls in the GPU framebuffer or register ranges. Other memory accesses continue
to use `ram.v`.

Example software write:

```cpp
volatile unsigned int* fb = (volatile unsigned int*)0x00020000;
fb[y * 160 + x] = 0x00FF0000; // red pixel
```

Programs that want a small drawing framework can include `Screen_SDK.h` and use
`Screen_Draw_Rectangle`, `Screen_Erase_Moved_Rectangle`, `Screen_Update`, and
`Screen_Delay`. `codes/build_riscv.sh` automatically links `Screen_SDK.cpp`
when it sees that include.

Related tests:

```bash
./run.sh gpu tb_gpu.cpp
./run.sh cpu tb_cpu_gpu.cpp
./codes/build_riscv.sh gpu_demo
./test.sh gpu_demo 0
```
