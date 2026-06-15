# control.v

`control.v` decodes RV32I opcode and function fields into control signals for
the rest of the CPU datapath.

## Interface

```verilog
module control (
    input  wire [6:0]  opcode,
    input  wire [2:0]  funct3,
    input  wire [6:0]  funct7,
    output reg         alu_src,
    output reg  [3:0]  alu_op,
    output reg         reg_write,
    output reg         mem_read,
    output reg         mem_write,
    output reg  [1:0]  mem_to_reg,
    output reg         branch,
    output reg         branch_inv,
    output reg         jump,
    output reg         jalr,
    output reg         pc_to_alu,
    output reg  [1:0]  mem_size,
    output reg         mem_unsigned
);
```

## Behavior

The control unit is combinational. It starts each decode with safe defaults, then
overrides outputs based on the opcode.

Supported instruction classes:

- R-type ALU operations: ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU.
- I-type ALU operations: ADDI, ANDI, ORI, XORI, SLLI, SRLI, SRAI, SLTI, SLTIU.
- Loads: LB, LH, LW, LBU, LHU.
- Stores: SB, SH, SW.
- Branches: BEQ, BNE, BLT, BGE, BLTU, BGEU.
- Jumps: JAL and JALR.
- Upper immediate instructions: LUI and AUIPC.
- SYSTEM instructions are treated as safe no-op/halt control defaults.

`branch_inv` is asserted for branch forms whose condition is the inverse of the
ALU predicate, such as BNE, BGE, and BGEU.

## Related C++ Files

`tb_control.cpp` is the Verilator testbench for this module. It instantiates
`Vcontrol`, defines expected control bundles, and checks every supported opcode
family. The testbench also checks the extra `branch_inv` output, so it reflects
the current Verilog interface rather than only the shorter interface shown in
older docs.

Typical command:

```bash
./run.sh control tb_control.cpp
```

