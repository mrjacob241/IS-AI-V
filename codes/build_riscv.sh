#!/usr/bin/env bash

cd codes

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <basename>"
    exit 1
fi

PROGRAM="$1"
CPP_FILE="src/${PROGRAM}.cpp"
ELF_FILE="lds/${PROGRAM}-rv32_ld.elf"
INSTR_FILE="instr/${PROGRAM}_instr.txt"
EXTRA_SOURCES=()

if grep -q 'Screen_SDK.h' "$CPP_FILE"; then
    EXTRA_SOURCES+=("../Screen_SDK.cpp")
fi
if grep -q 'IO_SDK.h' "$CPP_FILE"; then
    EXTRA_SOURCES+=("../IO_SDK.cpp")
fi

riscv64-unknown-elf-g++ \
    -march=rv32im \
    -mabi=ilp32 \
    -nostdlib \
    -nostartfiles \
    -T link.ld \
    "$CPP_FILE" \
    "${EXTRA_SOURCES[@]}" \
    -o "$ELF_FILE"

riscv64-linux-gnu-objdump -d "$ELF_FILE" > "$INSTR_FILE"
# Append raw section bytes so parse_rom.py can load .rodata correctly.
# (-d only disassembles .text; .rodata bytes come out as 16-bit "insns" that
# the 8-hex-digit regex misses.  -s emits every section as raw byte groups.)
riscv64-linux-gnu-objdump -s -j .text -j .rodata -j .srodata "$ELF_FILE" >> "$INSTR_FILE"

echo "Generated:"
echo "  $ELF_FILE"
echo "  $INSTR_FILE"

cd ..
