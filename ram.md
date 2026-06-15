# ram.v

`ram.v` implements a byte-addressable unified memory for RV32I simulation.
The same backing array is used for instruction fetch and data load/store.

## Interface

```verilog
module ram #(
    parameter BYTES = 131072
) (
    input  wire        clk,
    input  wire [31:0] iaddr,
    output wire [31:0] instr,
    input  wire [31:0] daddr,
    input  wire [31:0] wd,
    input  wire        we,
    input  wire [1:0]  size,
    input  wire        is_unsigned,
    output reg  [31:0] rd
);
```

## Behavior

The memory is a `BYTES`-entry byte array. The default size is 128 KiB.

- Instruction fetch is combinational and reads a 32-bit little-endian word from
  `iaddr`.
- Data reads are combinational and use `daddr`, `size`, and `is_unsigned`.
- Data writes are synchronous on the rising edge of `clk`.
- `size = 00` selects byte access.
- `size = 01` selects halfword access.
- `size = 10` or any other value selects word access.
- Byte and halfword loads can be sign-extended or zero-extended.
- Stores update only the selected byte lanes.

All bytes are initialized to zero in simulation.

## Related C++ Files

`tb_ram.cpp` is the Verilator testbench for this module. It instantiates `Vram`
and provides `store` and `load` helpers around the data port. The tests cover
word, halfword, and byte accesses, sign and zero extension, write-enable gating,
instruction-port reads from the same backing memory, combinational instruction
fetch, and coexistence of code and stack addresses.

Typical command:

```bash
./run.sh ram tb_ram.cpp
```

