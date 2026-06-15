# Verilator Build Script (`run.sh`)

This project uses a helper script called `run.sh` to automatically compile and execute Verilator simulations.

## Purpose

Instead of manually typing:

```bash
verilator --cc cpu.v alu.v decoder.v regfile.v --exe cpu.cpp --build
./obj_dir/Vcpu
```

the script automatically:

1. Collects all `.v` files in the current directory
2. Invokes Verilator
3. Builds the generated simulator
4. Runs the simulation

---

## Script

```bash
#!/usr/bin/env bash
set -e

TOP=${1:-cpu}
CPP=${2:-cpu.cpp}

rm -rf obj_dir

verilator \
    --cc \
    --top-module "$TOP" \
    ./*.v \
    --exe "$CPP" \
    --build

./obj_dir/V$TOP
```

---

## Installation

Make the script executable:

```bash
chmod +x run.sh
```

---

## Default Usage

For a project containing:

```text
cpu.v
alu.v
decoder.v
regfile.v
cpu.cpp
run.sh
```

run:

```bash
./run.sh
```

This is equivalent to:

```bash
verilator --cc --top-module cpu ./*.v --exe cpu.cpp --build
./obj_dir/Vcpu
```

---

## Selecting a Different Top Module

If testing a standalone component:

```text
alu.v
alu.cpp
```

run:

```bash
./run.sh alu alu.cpp
```

The script will execute:

```bash
verilator --cc --top-module alu ./*.v --exe alu.cpp --build
./obj_dir/Valu
```

---

## Project Layout

Recommended structure:

```text
project/
├── cpu.v
├── alu.v
├── decoder.v
├── regfile.v
├── cpu.cpp
├── run.sh
└── README.md
```

---

## Notes

* All Verilog files (`*.v`) in the current directory are compiled.
* The top module must match the module name passed to `run.sh`.
* Verilator generates build files in `obj_dir/`.
* `obj_dir/` is deleted before every build to ensure a clean compilation.

---

## Example Session

```bash
$ ./run.sh

Cycle=0 PC=1 x3=13 x4=0
Cycle=1 PC=2 x3=13 x4=7
Cycle=2 PC=3 x3=13 x4=7
```

This indicates that the simulated CPU successfully executed the loaded program.

