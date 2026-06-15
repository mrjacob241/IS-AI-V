# alu.v

`alu.v` implements the 32-bit RV32I arithmetic logic unit used by the CPU execute stage.

## Interface

```verilog
module alu (
    input  wire [31:0] a,
    input  wire [31:0] b,
    input  wire [3:0]  op,
    output reg  [31:0] y,
    output wire        zero
);
```

## Behavior

The ALU is purely combinational. Inputs `a`, `b`, and `op` determine result `y`;
`zero` is asserted when `y == 0`.

Operation codes:

| `op` | Operation | Meaning |
|------|-----------|---------|
| `0000` | ADD  | `a + b` |
| `0001` | SUB  | `a - b` |
| `0010` | AND  | `a & b` |
| `0011` | OR   | `a | b` |
| `0100` | XOR  | `a ^ b` |
| `0101` | SLL  | `a << b[4:0]` |
| `0110` | SRL  | logical right shift |
| `0111` | SRA  | arithmetic right shift |
| `1000` | SLT  | signed less-than |
| `1001` | SLTU | unsigned less-than |

Shifts use only the low five bits of `b`, matching RV32I.

## Related C++ Files

`tb_alu.cpp` is the Verilator testbench for this module. It instantiates
`Valu`, drives every ALU operation code, and checks the result plus the `zero`
flag. The tests cover arithmetic wraparound, logical operations, signed and
unsigned comparisons, arithmetic shifts, and masking of shift amounts to
`b[4:0]`.

Typical command:

```bash
./run.sh alu tb_alu.cpp
```

