#!/usr/bin/env bash
set -e

TOP=${1:-cpu}
CPP=${2:-cpu.cpp}
EXTRA_CPP=("${@:3}")

if [[ "$CPP" == "tb_live_screen.cpp" && ${#EXTRA_CPP[@]} -eq 0 ]]; then
    EXTRA_CPP=(Screen_SDK.cpp)
fi
if [[ "$CPP" == "tb_pong_test.cpp" && ${#EXTRA_CPP[@]} -eq 0 ]]; then
    EXTRA_CPP=(pong_input_io.cpp)
fi
if [[ "$CPP" == "tb_io.cpp" && ${#EXTRA_CPP[@]} -eq 0 ]]; then
    EXTRA_CPP=(pong_input_io.cpp)
fi

rm -rf obj_dir

verilator \
    --cc \
    --top-module "$TOP" \
    ./*.v \
    --exe "$CPP" "${EXTRA_CPP[@]}" \
    --build

./obj_dir/V$TOP
