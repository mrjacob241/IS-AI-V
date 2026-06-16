# GPU Rendering and Live Screen Pipeline

This document describes the current simulation GPU, how software writes pixels,
how the virtual 800x480 screen is produced, and how the C++ Verilator host code
captures it for live viewing.

The design is intentionally simple. It is not a full HDMI controller yet. It is
a small memory-mapped raster device connected to the single-cycle RV32I CPU.

---

## System Overview

The current screen path is:

```text
bare-metal C++ program
        |
        | RV32I stores
        v
cpu.v MMIO decode
        |
        v
gpu.v framebuffer, 160x120 source pixels
        |
        | nearest-neighbor hardware upscale
        v
virtual 800x480 RGB scanout signals
        |
        v
Verilator C++ host capture
        |
        v
screen.ppm
        |
        v
Python Tk PPM live viewer
```

The CPU does not know about a window, PPM file, or desktop display. It only
writes memory-mapped pixels and registers. The host-side C++ code observes the
virtual video output signals and turns them into an image file.

---

## Memory Map

The CPU routes normal load/store instructions to RAM, GPU, or IO based on the
computed byte address.

| Address range | Target | Description |
|---------------|--------|-------------|
| `0x00000000` - `0x0001FFFF` | `ram.v` | CPU program/data RAM |
| `0x00020000` - `0x00032BFF` | `gpu.v` | 160x120 framebuffer |
| `0x00040000` - `0x000400FF` | `gpu.v` | GPU registers |
| `0x00050000` - `0x000500FF` | `io.v` | Keyboard/input registers |

The framebuffer uses one 32-bit word per source pixel:

```text
0x00RRGGBB
```

Only the lower 24 bits are used by the GPU. The top byte is ignored.

The framebuffer has:

```text
width  = 160
height = 120
words  = 160 * 120 = 19200
bytes  = 19200 * 4 = 76800
```

The first pixel is at:

```text
0x00020000
```

Pixel `(x, y)` is:

```text
0x00020000 + ((y * 160 + x) * 4)
```

Because the CPU currently lacks RV32M multiply/divide support, the C++ demos use
shift/add math for `y * 160`:

```cpp
int row = (y << 7) + (y << 5); // y*128 + y*32 = y*160
fb[row + x] = color;
```

---

## GPU Registers

GPU registers live at `0x00040000`.

| Address | Access | Meaning |
|---------|--------|---------|
| `0x00040000` | read | Source framebuffer width, `160` |
| `0x00040004` | read | Source framebuffer height, `120` |
| `0x00040008` | read | Output width, `800` |
| `0x0004000C` | read | Output height, `480` |
| `0x00040010` | read | GPU scanout frame counter |
| `0x00040014` | read/write | Software debug counter, used by `pong_live` as the game-frame counter |

The software debug counter is useful for diagnosing whether the game simulation
and the graphics capture are advancing at similar rates. `pong_live` increments
it only when game physics advances.

Keyboard and typed-character input are handled by `io.v`, not by the GPU:

| Address | Meaning |
|---------|---------|
| `0x00050000` | Key-state bitmask |
| `0x00050004` | Current typed character |
| `0x00050008` | Typed-character sequence |
| `0x0005000C` | Typed-character acknowledge |

Bare-metal programs use `IO_SDK.h` / `IO_SDK.cpp` for the `Keyboard` object.

---

## CPU Integration

`cpu.v` decides whether a memory access goes to RAM, GPU, or IO based on the
ALU address for the current load/store.

For stores:

```text
if address is in GPU range:
    gpu.v receives mmio_we, mmio_addr, mmio_wd, mmio_size
else if address is in IO range:
    io.v receives mmio_we, mmio_addr, mmio_wd, mmio_size
else:
    ram.v receives the write
```

For loads:

```text
if address is in GPU range:
    writeback data comes from gpu_rd
else if address is in IO range:
    writeback data comes from io_rd
else:
    writeback data comes from ram_rd
```

This means bare-metal C++ can treat the framebuffer as a normal volatile pointer:

```cpp
static volatile unsigned int* const fb =
    (volatile unsigned int*)0x00020000;

fb[y * 160 + x] = 0x00FFCC30;
```

The `volatile` is important. It prevents the compiler from deleting or
reordering framebuffer writes as ordinary dead memory stores.

---

## gpu.v Framebuffer

`gpu.v` owns a framebuffer:

```verilog
reg [23:0] fb [0:FB_PIXELS-1];
```

The CPU writes it through the MMIO path:

```verilog
if (mmio_we && fb_addr_hit && (mmio_size == 2'b10))
    fb[fb_index] <= mmio_wd[23:0];
```

The first version supports word stores for pixels. Byte and halfword pixel
stores are not implemented.

The framebuffer is initialized to black in simulation:

```verilog
for (_i = 0; _i < FB_PIXELS; _i = _i + 1)
    fb[_i] = 24'b0;
```

This is why the Pong demo can start from a black background without explicitly
clearing every pixel.

---

## Hardware Upscaling

The software framebuffer is intentionally small:

```text
160x120
```

The virtual output is:

```text
800x480
```

The scale factors are:

```text
horizontal: 800 / 160 = 5
vertical:   480 / 120 = 4
```

So one source pixel becomes a 5x4 block on the virtual screen.

The GPU does nearest-neighbor upscaling. It keeps output counters and source
pixel counters:

```verilog
out_x   // 0..799
out_y   // 0..479
src_x   // 0..159
src_y   // 0..119
scale_x // 0..4
scale_y // 0..3
```

On each output pixel tick:

```text
video_rgb = fb[src_y * 160 + src_x]
```

Then `out_x` advances. `src_x` advances only after five output pixels. At the
end of each output line, `out_y` advances. `src_y` advances only after four
output lines.

This avoids hardware division:

```text
bad for hardware: src_x = out_x / 5
current design:   count repeated pixels with scale_x
```

---

## Virtual Video Signals

`gpu.v` exports a simple pre-HDMI RGB stream:

```verilog
video_rgb
video_hsync
video_vsync
video_de
video_x
video_y
video_frame_done
```

Meanings:

| Signal | Meaning |
|--------|---------|
| `video_rgb` | Current output pixel, 24-bit RGB |
| `video_hsync` | Simple line marker, asserted at `x == 0` |
| `video_vsync` | Simple frame marker, asserted at `x == 0 && y == 0` |
| `video_de` | Data enable; currently always high outside reset |
| `video_x` | Output x coordinate, `0..799` |
| `video_y` | Output y coordinate, `0..479` |
| `video_frame_done` | Pulses when the scanout wraps from the last pixel to the first |

This is not real HDMI/TMDS. It is the RGB/sync/data-enable stream that would
sit before an HDMI/DVI encoder.

---

## Host-Side C++ Capture

There are two host-side C++ screen programs.

### tb_screen.cpp

`tb_screen.cpp` captures one frame and writes `screen.ppm`.

Typical use:

```bash
./codes/build_riscv.sh pong
python3 parse_rom.py codes/instr/pong_instr.txt
./run.sh cpu tb_screen.cpp
```

It:

1. Holds reset.
2. Lets `cpu.v` load `rom_dump.v` into RAM.
3. Initializes `sp` to `0x0001FFF0`.
4. Releases reset.
5. Runs until `ecall`.
6. Waits for the next virtual frame start.
7. Captures 800x480 RGB pixels.
8. Writes `screen.ppm`.

### tb_live_screen.cpp

`tb_live_screen.cpp` continuously captures frames and rewrites `screen.ppm`
atomically by delegating to `ScreenSDK::run_live_capture(...)`. It is
intentionally thin: `BuildLiveConfig(...)` reads the output path plus
`LIVE_FRAMES` / `LIVE_FPS`, while the reusable capture, PPM writing, Ctrl+C
handling, FPS pacing, and status printing live in `Screen_SDK.cpp` with
declarations in `Screen_SDK.h`.

`Screen_SDK` is also the bare-metal drawing framework used by games. The same
source file is compiled two ways:

```text
host build:   Verilator live capture helpers
RISC-V build: framebuffer drawing helpers
```

The RISC-V side exposes C-linkage functions:

```cpp
Screen_Draw_Rectangle(x, y, w, h, color);
Screen_Erase_Moved_Rectangle(old_x, old_y, new_x, new_y, w, h);
Screen_Update(game_frame);
Screen_Delay(cycles);
Screen_Clear(color);
```

`codes/build_riscv.sh` automatically links `../Screen_SDK.cpp` when a program
includes `Screen_SDK.h`. This is how `pong_live` gets the drawing framework
without manually listing extra sources.

Typical use:

```bash
./codes/build_riscv.sh pong_live
python3 parse_rom.py codes/instr/pong_live_instr.txt
./run.sh cpu tb_live_screen.cpp
```

It runs until Ctrl+C.

`run.sh` automatically adds `Screen_SDK.cpp` when the testbench is
`tb_live_screen.cpp`. You can also pass extra host C++ files explicitly:

```bash
./run.sh cpu tb_live_screen.cpp Screen_SDK.cpp
```

By default it publishes PPM files at 30 FPS:

```bash
LIVE_FPS=30 ./run.sh cpu tb_live_screen.cpp
```

You can run a finite number of frames for testing:

```bash
LIVE_FRAMES=5 ./run.sh cpu tb_live_screen.cpp
```

The writer uses a temporary file and `rename()`:

```text
screen.ppm.tmp -> screen.ppm
```

This avoids the Python viewer reading a half-written PPM file.

The live bridge prints both graphics and game counters:

```text
gfx_frame=3 game_frame=2 game_delta=1 -> screen.ppm
```

Where:

| Field | Meaning |
|-------|---------|
| `gfx_frame` | Number of captured scanout frames written by host C++ |
| `game_frame` | Counter written by the game into GPU register `0x00040014` |
| `game_delta` | How many game updates happened since the previous captured graphics frame |

For smooth animation, `game_delta` should usually be around `1`. Larger values
mean the game simulation is advancing faster than the viewer captures frames,
which looks like skipped animation.

---

## Python PPM Live Viewer

`ppm_live_viewer.py` is a tiny Tkinter viewer.

Run it in a separate terminal:

```bash
python3 ppm_live_viewer.py screen.ppm
```

It polls the file timestamp and reloads the image when `screen.ppm` changes.
The poll interval is 33 ms, matching the default 30 FPS feed.

It does not talk to Verilator directly. It only watches the PPM file.

---

## Pong Programs

There are three Pong variants.

### pong.cpp

`pong.cpp` is a finite smoke test. It draws a Pong scene and exits with
`a0 = 0`.

Run:

```bash
./codes/build_riscv.sh pong
./test.sh pong 0
```

This is useful for regression testing because it terminates.

### pong_live.cpp

`pong_live.cpp` is the live animation version. It loops forever and is intended
to be stopped by Ctrl+C in the Verilator host process.

Run with live viewing:

```bash
python3 ppm_live_viewer.py screen.ppm
```

In another terminal:

```bash
./codes/build_riscv.sh pong_live
python3 parse_rom.py codes/instr/pong_live_instr.txt
./run.sh cpu tb_live_screen.cpp
```

Stop the simulator with Ctrl+C.

---

### pong_test.cpp

`pong_test.cpp` is the player-controlled live version. The left paddle is
controlled through `IO_SDK` keyboard input (`W`/`S`), while the right paddle
tracks the ball automatically. If the ball crosses the player's left boundary
without paddle contact, the framebuffer is cleared red, the game pauses briefly,
then the ball and paddles reset to their starting positions.

Run with live viewing:

```bash
python3 ppm_live_viewer.py screen.ppm
```

In another terminal:

```bash
./codes/build_riscv.sh pong_test
python3 parse_rom.py codes/instr/pong_test_instr.txt
./run.sh cpu tb_pong_test.cpp
```

Controls: `W` = up, `S` = down, `Q` = quit the host simulator.

---

## Screen SDK

`Screen_SDK.h` / `Screen_SDK.cpp` is the small graphics framework shared by
bare-metal programs and host-side Verilator screen capture.

For bare-metal RISC-V programs:

```cpp
#include "../../Screen_SDK.h"
```

Available drawing functions:

| Function | Purpose |
|----------|---------|
| `Screen_Draw_Rectangle(x, y, w, h, color)` | Fill a rectangle in the 160x120 framebuffer |
| `Screen_Erase_Moved_Rectangle(old_x, old_y, new_x, new_y, w, h)` | Erase only exposed pixels when an object moves |
| `Screen_Clear(color)` | Fill the whole source framebuffer |
| `Screen_Update(game_frame)` | Publish a game-frame counter to GPU register `0x00040014` |
| `Screen_Delay(cycles)` | Busy-wait delay for simple demos |

Available color constants:

```cpp
SCREEN_COLOR_BLACK
SCREEN_COLOR_WHITE
SCREEN_COLOR_GRAY
SCREEN_COLOR_BORDER
SCREEN_COLOR_YELLOW
```

For host-side Verilator programs, the same SDK provides:

```cpp
ScreenSDK::run_live_capture(config);
ScreenSDK::env_int_allow_zero(name, fallback);
```

`run.sh` automatically compiles `Screen_SDK.cpp` for `tb_live_screen.cpp`.
`codes/build_riscv.sh` automatically compiles `../Screen_SDK.cpp` for
bare-metal programs that include `Screen_SDK.h`.

---

## Pong Rendering Strategy

The Pong renderer uses dirty rectangle drawing instead of full-screen redraws.

Full-screen redraw of 160x120 would mean:

```text
19200 framebuffer writes per frame
```

That is expensive for the current unoptimized bare-metal build and single-cycle
simulation. Instead, the game draws only:

```text
top/bottom border
center net
left paddle
right paddle
ball
```

The background starts black because `gpu.v` initializes the framebuffer to zero.

### Drawing Rectangles

The core primitive now lives in `Screen_SDK.cpp` and is called as:

```cpp
Screen_Draw_Rectangle(x, y, w, h, color);
```

Internally, the SDK computes each row as `py * 160` without multiplication:

```cpp
int row = (py << 7) + (py << 5);
```

### Avoiding Flicker and Shrinking Objects

The GPU scans the framebuffer while the CPU writes it. There is no double
buffering yet. That means scanout can observe partially updated objects.

The first Pong implementation erased the whole old paddle and then drew the new
paddle. If the scanout reached that area between those operations, the viewer
saw black bands or a shrinking paddle.

The current code uses:

```cpp
Screen_Erase_Moved_Rectangle(old_x, old_y, new_x, new_y, w, h);
```

This function erases only the parts of the old rectangle that are not overlapped
by the new rectangle.

For example, if a paddle moves down by one source pixel:

```text
old paddle:
  y=50..69

new paddle:
  y=51..70

overlap:
  y=51..69

erase only:
  y=50
```

Then the full new paddle is redrawn. This greatly reduces the time during which
the framebuffer contains a visible hole.

This is still not perfect in principle. The robust hardware fix is double
buffering or a CPU/GPU synchronization point during vertical blank. The current
design avoids the worst tearing while staying simple.

### Game Timing

`pong_live` has two timing mechanisms:

1. `delay_frame()` burns CPU cycles to slow the loop.
2. `move_divider` gates physics updates so the ball does not move every loop.

Current logic:

```cpp
if (move_divider >= 89) {
    move_divider = 0;
    // update ball/paddles and increment game_frame
} else {
    move_divider++;
}
```

The game writes:

```cpp
*gpu_game_frame = game_frame;
```

The host prints that counter beside each captured graphics frame. This makes it
easy to tune animation speed.

If motion is too fast:

```cpp
move_divider >= 119
```

If motion is too slow:

```cpp
move_divider >= 59
```

The exact value depends on CPU program cost and scanout/capture rate.

---

## Current Limitations

The current GPU/rendering path is good for simulation experiments, but it is
not yet a polished FPGA display subsystem.

Important limitations:

- No double buffering.
- No vertical blank interrupt or synchronization.
- No real blanking intervals.
- No TMDS HDMI encoder.
- `video_de` is always high during active simulation.
- CPU and GPU share the framebuffer without arbitration beyond simple Verilog
  memory semantics.
- The framebuffer is 24-bit, which is larger than ideal for a small FPGA.
- The C++ build uses no optimization by default, so helper-heavy rendering code
  can be slow.

For an FPGA version, the likely next steps are:

1. Convert the framebuffer to RGB565.
2. Use explicit FPGA-friendly dual-port block RAM.
3. Add a real 800x480 timing generator with blanking.
4. Add vblank or double buffering.
5. Add board-specific DVI/HDMI output for iCESugar Pro ECP5.

---

## Useful Commands

Finite Pong smoke test:

```bash
./codes/build_riscv.sh pong
./test.sh pong 0
```

Single-frame screenshot:

```bash
./codes/build_riscv.sh pong
python3 parse_rom.py codes/instr/pong_instr.txt
./run.sh cpu tb_screen.cpp
```

Live Pong:

```bash
python3 ppm_live_viewer.py screen.ppm
```

In another terminal:

```bash
./codes/build_riscv.sh pong_live
python3 parse_rom.py codes/instr/pong_live_instr.txt
./run.sh cpu tb_live_screen.cpp
```

Finite live capture for debugging:

```bash
LIVE_FRAMES=10 LIVE_FPS=30 ./run.sh cpu tb_live_screen.cpp
```

Check for unsupported RV32M instructions:

```bash
rg -n "\b(mul|div|rem|mulh|mulhu|mulhsu)\b" codes/instr/pong_live_instr.txt
```

If that command prints nothing, the program did not emit those unsupported
instructions.
