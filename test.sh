#!/usr/bin/env bash
# Usage: ./test.sh <program_name> <expected_a0>
#   program_name  — basename under codes/src/ and codes/instr/
#   expected_a0   — decimal or hex (0x...) expected value of a0 at ecall
#
# Steps: parse_rom → verilator rebuild → run → PASS/FAIL

set -e

NAME=${1:?'Usage: ./test.sh <name> <expected_a0>'}
EXPECTED=${2:?'Usage: ./test.sh <name> <expected_a0>'}

INSTR="codes/instr/${NAME}_instr.txt"

if [[ ! -f "$INSTR" ]]; then
    echo "ERROR: instruction file not found: $INSTR" >&2
    echo "       Run: ./codes/build_riscv.sh $NAME" >&2
    exit 1
fi

echo "--- $NAME (expected a0=$EXPECTED) ---"
python3 parse_rom.py "$INSTR"
TEST_NAME="$NAME" EXPECTED_A0="$EXPECTED" ./run.sh cpu tb_program.cpp
