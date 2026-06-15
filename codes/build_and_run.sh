#!/usr/bin/env bash

cd codes

set -uo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <basename>"
    exit 1
fi

PROGRAM="$1"
CPP_FILE="src/${PROGRAM}.cpp"
ELF_FILE="builds/${PROGRAM}-rv32.elf"

riscv64-unknown-elf-g++ \
    -march=rv32im \
    -mabi=ilp32 \
    -nostdlib \
    -nostartfiles \
    "$CPP_FILE" \
    -o "$ELF_FILE"

qemu-riscv32 "./$ELF_FILE" | cat
echo "${PIPESTATUS[0]}"

cd ..
