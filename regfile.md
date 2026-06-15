# regfile.v

`regfile.v` implements the RV32I integer register file.

## Interface

```verilog
module regfile (
    input  wire        clk,
    input  wire [4:0]  rs1,
    input  wire [4:0]  rs2,
    input  wire [4:0]  rd,
    input  wire [31:0] wd,
    input  wire        we,
    output wire [31:0] a,
    output wire [31:0] b
);
```

## Behavior

The module provides two combinational read ports and one synchronous write port.

- `rs1` selects read output `a`.
- `rs2` selects read output `b`.
- `rd`, `wd`, and `we` control the write port.
- Writes occur on the rising edge of `clk`.
- Register `x0` always reads as zero and ignores writes.
- All registers are initialized to zero in simulation.

The storage array is `regs[0:31]`, which also makes it convenient for debug
access from a top-level CPU module.

## Related C++ Files

`tb_regfile.cpp` is the Verilator testbench for this module. It instantiates
`Vregfile`, provides helper functions for clock ticks and register writes, and
checks basic writes, `x0` behavior, write-enable gating, two-port reads, and
independence of registers `x1` through `x31`.

Typical command:

```bash
./run.sh regfile tb_regfile.cpp
```

