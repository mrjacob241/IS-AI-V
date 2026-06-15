# imm_gen.v

`imm_gen.v` extracts and sign-extends immediates from RV32I instruction words.

## Interface

```verilog
module imm_gen (
    input  wire [31:0] instr,
    output reg  [31:0] imm
);
```

## Behavior

The module is combinational. It selects the immediate format from
`instr[6:0]`, the opcode field.

Supported formats:

| Opcode class | Immediate encoding |
|--------------|--------------------|
| I-type ALU, loads, JALR, SYSTEM | `instr[31:20]` sign-extended |
| S-type stores | `{instr[31:25], instr[11:7]}` sign-extended |
| B-type branches | scrambled branch offset with low bit forced to zero |
| U-type LUI/AUIPC | `instr[31:12] << 12` |
| J-type JAL | scrambled jump offset with low bit forced to zero |

Unknown opcodes produce `imm = 0`.

## Related C++ Files

`tb_imm_gen.cpp` is the Verilator testbench for this module. It instantiates
`Vimm_gen`, feeds exact RV32I instruction encodings, and checks the generated
immediate for positive and negative I, S, B, U, and J encodings.

Typical command:

```bash
./run.sh imm_gen tb_imm_gen.cpp
```

