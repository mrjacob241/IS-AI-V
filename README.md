# IS-AI-V
# ⚠️ WARNING: Vibe Coded Project ⚠️

IS-AI-V it's a fully AI designed RISC-V CPU, built for meme not for performance.

## Requirements

Run these commands from the repository root.

You need:

- Verilator
- A RISC-V GCC toolchain that can build RV32I bare-metal programs
- Python 3
- Tkinter for the live PPM viewer, if you want to run the live Pong screen

## Calculator Sample

The calculator is an interactive two-number adder implemented in
`codes/src/calc.cpp`. It reads keyboard input through `IO_SDK` / `io.v` and
prints through `ecall` output handled by `tb_io.cpp`.

Build the RISC-V program:

```bash
./codes/build_riscv.sh calc
```

Generate the Verilog ROM:

```bash
python3 parse_rom.py codes/instr/calc_instr.txt
```

Run it on the CPU simulator:

```bash
./run.sh cpu tb_io.cpp
```

At the prompts, enter two integers. Type `q` at either prompt to quit.

## Pong Sample

### Finite Smoke Test

This builds and runs the finite Pong sample. It draws a Pong scene through the
GPU framebuffer and exits with `a0 = 0`.

```bash
./codes/build_riscv.sh pong
./test.sh pong 0
```

### Single Screenshot

This captures one simulated 800x480 frame into `screen.ppm`.

```bash
./codes/build_riscv.sh pong
python3 parse_rom.py codes/instr/pong_instr.txt
./run.sh cpu tb_screen.cpp
```

Open `screen.ppm` with any image viewer that supports PPM files.

### Live Pong

Start the live PPM viewer in one terminal:

```bash
python3 ppm_live_viewer.py screen.ppm
```

In another terminal, build and run the live Pong program:

```bash
./codes/build_riscv.sh pong_live
python3 parse_rom.py codes/instr/pong_live_instr.txt
./run.sh cpu tb_live_screen.cpp
```

Stop the simulator with `Ctrl+C`.

Optional finite live capture for debugging:

```bash
LIVE_FRAMES=10 LIVE_FPS=30 ./run.sh cpu tb_live_screen.cpp
```

### Playable Pong

Start the live PPM viewer in one terminal:

```bash
python3 ppm_live_viewer.py screen.ppm
```

In another terminal, build and run the player-controlled Pong program:

```bash
./codes/build_riscv.sh pong_test
python3 parse_rom.py codes/instr/pong_test_instr.txt
./run.sh cpu tb_pong_test.cpp
```

Controls: `W` = up, `S` = down, `Q` = quit the host simulator.
If the ball passes the player's left boundary, the screen flashes red and the
game resets to the initial ball and paddle positions.
