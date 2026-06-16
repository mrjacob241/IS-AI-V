# RV32I CPU — Unit Test Specification

Build order mirrors the dependency graph: each component is tested in isolation
before being wired into the next layer. The final integration test runs a real
compiled RV32I binary.

---

## Memory architecture

Single unified Von Neumann RAM — one backing byte array for both instruction
fetch and data load/store. This is the only design that works for arbitrary
compiled code without relinking.

```
Address space: 0x00000000 – 0x0001FFFF  (128 KB)
  └── code loaded at 0x00000000 (bottom)
  └── stack grows down from 0x0001FFF0  (top)
```

The module exposes two independent read paths into the same array:
- **instruction port** (`iaddr` / `instr`): combinational, used by the fetch stage
- **data port** (`daddr` / `rd` / `wd`): combinational read, synchronous write

The testbench is responsible for pre-initialising `sp` (x2) to `0x0001FFF0`
by writing directly into the register file **before** releasing reset.
This mirrors what a real boot loader or OS does before jumping to `_start`.
Without this, the very first compiled instruction (`addi sp, sp, -N`) wraps
the stack pointer to near `0xFFFFFFFF`.

---

## Component build order

```
1. regfile.v        → tb_regfile.cpp
2. alu.v            → tb_alu.cpp
3. imm_gen.v        → tb_imm_gen.cpp
4. control.v        → tb_control.cpp
5. ram.v            → tb_ram.cpp       (unified instruction + data memory)
6. cpu.v            → tb_cpu.cpp       (integration)
```

---

## Signal conventions used throughout

### ALU operation codes (`alu_op[3:0]`)

```
4'b0000  ADD
4'b0001  SUB
4'b0010  AND
4'b0011  OR
4'b0100  XOR
4'b0101  SLL   (shift left logical)
4'b0110  SRL   (shift right logical)
4'b0111  SRA   (shift right arithmetic)
4'b1000  SLT   (set less than, signed)
4'b1001  SLTU  (set less than, unsigned)
```

### Control unit output signals

| Signal        | Width | Meaning                                              |
|---------------|-------|------------------------------------------------------|
| `alu_src`     | 1     | 0 = B input from rs2 register; 1 = B from immediate |
| `alu_op`      | 4     | ALU operation code (table above)                     |
| `reg_write`   | 1     | 1 = write result to rd                               |
| `mem_read`    | 1     | 1 = read data memory                                 |
| `mem_write`   | 1     | 1 = write data memory                                |
| `mem_to_reg`  | 2     | 00 = ALU result; 01 = mem data; 10 = PC+4            |
| `branch`      | 1     | 1 = conditional branch instruction                   |
| `jump`        | 1     | 1 = unconditional jump (JAL or JALR)                 |
| `jalr`        | 1     | 1 = JALR specifically (target = rs1+imm, not PC+imm) |
| `pc_to_alu`   | 1     | 1 = substitute PC for rs1 (AUIPC)                   |
| `mem_size`    | 2     | 00 = byte, 01 = halfword, 10 = word                  |
| `mem_unsigned`| 1     | 1 = zero-extend load result                          |

---

## 1. Register File (`regfile.v`)

### Interface

```verilog
module regfile (
    input  wire        clk,
    input  wire [4:0]  rs1,
    input  wire [4:0]  rs2,
    input  wire [4:0]  rd,
    input  wire [31:0] wd,
    input  wire        we,
    output wire [31:0] a,    // rs1 data
    output wire [31:0] b     // rs2 data
);
```

Reads are combinational. Writes are synchronous (posedge clk, when we=1).
`x0` must always read as 0 even when written.
No reset pin needed — x0 hardwired, all others start at undefined (testbench
can initialise to 0 via `initial`).

### Tests

**RF-01 — basic write then read**
- Clock posedge with `we=1, rd=1, wd=0xDEADBEEF`
- Set `rs1=1`
- Assert `a == 0xDEADBEEF`

**RF-02 — x0 always zero**
- Clock posedge with `we=1, rd=0, wd=0xDEADBEEF`
- Set `rs1=0`
- Assert `a == 0x00000000`

**RF-03 — write enable gating**
- Set `we=0, rd=2, wd=0xCAFEBABE` and clock
- Set `rs1=2`
- Assert `a` is not `0xCAFEBABE` (register unchanged)

**RF-04 — simultaneous two-port read**
- Write `x1=10` then `x2=3` (two clock cycles)
- Set `rs1=1, rs2=2`
- Assert `a==10` and `b==3` in the same combinational read

**RF-05 — write to one register does not disturb another**
- Write `x3=100`
- Write `x4=200`
- Read `rs1=3, rs2=4`
- Assert `a==100, b==200`

**RF-06 — all 32 registers independent**
- Loop i=1..31: write value `i*4` to `xi`
- Loop i=1..31: read `rsi`, assert `a == i*4`

---

## 2. ALU (`alu.v`)

### Interface

```verilog
module alu (
    input  wire [31:0] a,
    input  wire [31:0] b,
    input  wire [3:0]  op,
    output wire [31:0] y,
    output wire        zero    // (y == 0)
);
```

All operations are 32-bit. Shifts use only `b[4:0]`. SRA is arithmetic
(sign-extends). SLT/SLTU write 1 or 0 into `y`.

### Tests

**ALU-01 — ADD basic**
- `a=5, b=3, op=ADD` → `y=8, zero=0`

**ALU-02 — ADD overflow wraps**
- `a=0xFFFFFFFF, b=1, op=ADD` → `y=0x00000000, zero=1`

**ALU-03 — SUB basic**
- `a=10, b=3, op=SUB` → `y=7, zero=0`

**ALU-04 — SUB same operands (zero flag)**
- `a=5, b=5, op=SUB` → `y=0, zero=1`

**ALU-05 — AND**
- `a=0xFF00FF00, b=0xF0F0F0F0, op=AND` → `y=0xF000F000`

**ALU-06 — OR**
- `a=0xFF000000, b=0x00FF0000, op=OR` → `y=0xFFFF0000`

**ALU-07 — XOR**
- `a=0xFFFFFFFF, b=0x0F0F0F0F, op=XOR` → `y=0xF0F0F0F0`

**ALU-08 — SLL**
- `a=1, b=4, op=SLL` → `y=16`
- `a=1, b=31, op=SLL` → `y=0x80000000`

**ALU-09 — SRL (logical, fills with 0)**
- `a=0x80000000, b=1, op=SRL` → `y=0x40000000`
- `a=0xFFFFFFFF, b=4, op=SRL` → `y=0x0FFFFFFF`

**ALU-10 — SRA (arithmetic, fills with sign bit)**
- `a=0x80000000, b=1, op=SRA` → `y=0xC0000000`
- `a=0xFFFFFFFF, b=4, op=SRA` → `y=0xFFFFFFFF`

**ALU-11 — SLT signed**
- `a=3, b=5, op=SLT` → `y=1`
- `a=5, b=3, op=SLT` → `y=0`
- `a=0xFFFFFFFF, b=0, op=SLT` → `y=1`  (0xFFFFFFFF = −1 signed; −1 < 0 = true)

**ALU-12 — SLTU unsigned**
- `a=0xFFFFFFFF, b=0, op=SLTU` → `y=0` (0xFFFFFFFF > 0 unsigned)
- `a=0, b=0xFFFFFFFF, op=SLTU` → `y=1`

**ALU-13 — shift ignores upper bits of b**
- `a=1, b=0xFFFFFFE1 (lower 5 bits = 1), op=SLL` → `y=2`

**ALU-14 — zero flag wires**
- `a=7, b=7, op=SUB` → `zero=1`
- `a=7, b=6, op=SUB` → `zero=0`

---

## 3. Immediate Generator (`imm_gen.v`)

### Interface

```verilog
module imm_gen (
    input  wire [31:0] instr,
    output wire [31:0] imm
);
```

Extracts and sign-extends the immediate for all five encoding formats.
The format is selected from `instr[6:0]` (opcode).

### Tests (instruction encodings are exact 32-bit hex values)

**IMM-01 — I-type positive**
- Instruction: `addi x1, x0, 5`  → `32'h00500093`
- Expected `imm = 32'h00000005`

**IMM-02 — I-type negative (sign extension)**
- Instruction: `addi x1, x0, -1` → `32'hFFF00093`
- Expected `imm = 32'hFFFFFFFF`

**IMM-03 — I-type load**
- Instruction: `lw x1, 4(x2)` → `32'h00412083`
- Expected `imm = 32'h00000004`

**IMM-04 — I-type JALR**
- Instruction: `jalr x1, 0(x5)` → `32'h000280E7`
- Expected `imm = 32'h00000000`

**IMM-05 — S-type positive**
- Instruction: `sw x1, 12(x2)` → `32'h00112623`
- Expected `imm = 32'h0000000C`

**IMM-06 — S-type negative offset**
- Instruction: `sw x1, -4(x2)` → `32'hFE112E23`
- Expected `imm = 32'hFFFFFFFC`

**IMM-07 — B-type positive offset**
- Instruction: `beq x1, x2, +8` → `32'h00208463`
- Expected `imm = 32'h00000008`

**IMM-08 — B-type negative offset**
- Instruction: `beq x1, x2, -8` → `32'hFE208CE3`
- Expected `imm = 32'hFFFFFFF8`

**IMM-09 — U-type LUI**
- Instruction: `lui x1, 0x12345` → `32'h123452B7`
- Expected `imm = 32'h12345000`

**IMM-10 — U-type AUIPC**
- Instruction: `auipc x1, 1` → `32'h00001097`
- Expected `imm = 32'h00001000`

**IMM-11 — J-type JAL positive**
- Instruction: `jal x1, +12` → `32'h00C000EF`
- Expected `imm = 32'h0000000C`

**IMM-12 — J-type JAL negative**
- Instruction: `jal x0, -4` → `32'hFFDFF06F`
- Expected `imm = 32'hFFFFFFFC`

---

## 4. Control Unit (`control.v`)

### Interface

```verilog
module control (
    input  wire [6:0]  opcode,
    input  wire [2:0]  funct3,
    input  wire [6:0]  funct7,
    output wire        alu_src,
    output wire [3:0]  alu_op,
    output wire        reg_write,
    output wire        mem_read,
    output wire        mem_write,
    output wire [1:0]  mem_to_reg,
    output wire        branch,
    output wire        jump,
    output wire        jalr,
    output wire        pc_to_alu,
    output wire [1:0]  mem_size,
    output wire        mem_unsigned
);
```

Inputs are taken directly from the decoded instruction fields. The testbench
feeds individual fields rather than a full instruction word.

### Tests

**CTRL-01 — R-type ADD** (`opcode=7'b0110011, funct3=3'b000, funct7=7'b0000000`)
- `alu_src=0, reg_write=1, mem_read=0, mem_write=0, mem_to_reg=2'b00, branch=0, jump=0`
- `alu_op=4'b0000` (ADD)

**CTRL-02 — R-type SUB** (`funct7=7'b0100000, funct3=3'b000`)
- `alu_op=4'b0001` (SUB), all others same as CTRL-01

**CTRL-03 — R-type AND/OR/XOR/SLL/SRL/SRA/SLT/SLTU**
- For each funct3 (and funct7 where needed), assert correct `alu_op`

| funct7    | funct3 | alu_op |
|-----------|--------|--------|
| 0000000   | 000    | 0000 ADD |
| 0100000   | 000    | 0001 SUB |
| 0000000   | 111    | 0010 AND |
| 0000000   | 110    | 0011 OR  |
| 0000000   | 100    | 0100 XOR |
| 0000000   | 001    | 0101 SLL |
| 0000000   | 101    | 0110 SRL |
| 0100000   | 101    | 0111 SRA |
| 0000000   | 010    | 1000 SLT |
| 0000000   | 011    | 1001 SLTU|

**CTRL-04 — I-type ADDI** (`opcode=7'b0010011, funct3=3'b000`)
- `alu_src=1, reg_write=1, mem_read=0, mem_write=0, mem_to_reg=2'b00, branch=0, jump=0`
- `alu_op=4'b0000` (ADD)

**CTRL-05 — I-type ANDI/ORI/XORI/SLTI/SLTIU/SLLI/SRLI/SRAI**
- `alu_src=1`, `reg_write=1`, correct `alu_op` per funct3/funct7

**CTRL-06 — LW** (`opcode=7'b0000011, funct3=3'b010`)
- `alu_src=1, reg_write=1, mem_read=1, mem_write=0, mem_to_reg=2'b01, branch=0, jump=0`
- `alu_op=4'b0000` (ADD, computes address)
- `mem_size=2'b10, mem_unsigned=0`

**CTRL-07 — LB / LBU / LH / LHU**
- `funct3=3'b000` (LB):  `mem_size=2'b00, mem_unsigned=0`
- `funct3=3'b100` (LBU): `mem_size=2'b00, mem_unsigned=1`
- `funct3=3'b001` (LH):  `mem_size=2'b01, mem_unsigned=0`
- `funct3=3'b101` (LHU): `mem_size=2'b01, mem_unsigned=1`

**CTRL-08 — SW** (`opcode=7'b0100011, funct3=3'b010`)
- `alu_src=1, reg_write=0, mem_read=0, mem_write=1, branch=0, jump=0`
- `alu_op=4'b0000`
- `mem_size=2'b10`

**CTRL-09 — BEQ** (`opcode=7'b1100011, funct3=3'b000`)
- `alu_src=0, reg_write=0, mem_read=0, mem_write=0, branch=1, jump=0`
- `alu_op=4'b0001` (SUB, compare via zero flag)

**CTRL-10 — BNE / BLT / BGE / BLTU / BGEU**
- All: `branch=1, reg_write=0, jump=0`
- `alu_op` per branch type:
  - BNE (001): SUB (check zero=0)
  - BLT (100): SLT
  - BGE (101): SLT (check result=0)
  - BLTU (110): SLTU
  - BGEU (111): SLTU (check result=0)

**CTRL-11 — JAL** (`opcode=7'b1101111`)
- `reg_write=1, mem_to_reg=2'b10 (PC+4), jump=1, jalr=0, branch=0`

**CTRL-12 — JALR** (`opcode=7'b1100111`)
- `reg_write=1, mem_to_reg=2'b10, jump=1, jalr=1, branch=0, alu_src=1, alu_op=4'b0000`

**CTRL-13 — LUI** (`opcode=7'b0110111`)
- `reg_write=1, alu_src=1, alu_op=4'b0000, mem_read=0, mem_write=0, branch=0, jump=0, pc_to_alu=0`
- (rs1 field is 0 for LUI; ALU computes 0 + imm = imm)

**CTRL-14 — AUIPC** (`opcode=7'b0010111`)
- `reg_write=1, alu_src=1, alu_op=4'b0000, pc_to_alu=1, branch=0, jump=0`

---

## 5. Unified RAM (`ram.v`)

### Interface

```verilog
module ram #(
    parameter WORDS = 32768   // 128 KB  (32768 × 4 bytes)
) (
    input  wire        clk,
    // instruction fetch port — combinational read
    input  wire [31:0] iaddr,
    output wire [31:0] instr,
    // data port — combinational read, synchronous byte-enable write
    input  wire [31:0] daddr,
    input  wire [31:0] wd,
    input  wire        we,
    input  wire [1:0]  size,         // 00=byte  01=half  10=word
    input  wire        is_unsigned,
    output wire [31:0] rd
);
```

- Byte-addressable, little-endian.
- Both read ports are combinational (address → data, no clock needed).
- Write is synchronous (posedge clk), byte-enable masked by `size`.
- Both ports address the same backing array; a write on the data port is
  visible on the instruction port in the next cycle (single-cycle CPU never
  needs this, but it must not break).

### Tests

**RAM-01 — word store and load (data port)**
- `we=1, daddr=0x100, wd=0xDEADBEEF, size=2'b10` → clock
- `daddr=0x100, size=2'b10` → `rd=0xDEADBEEF`

**RAM-02 — byte load signed (LB)**
- After RAM-01: `daddr=0x100, size=2'b00, is_unsigned=0`
- Little-endian byte 0 of 0xDEADBEEF = 0xEF
- `rd = 0xFFFFFFEF`

**RAM-03 — byte load unsigned (LBU)**
- After RAM-01: `daddr=0x100, size=2'b00, is_unsigned=1`
- `rd = 0x000000EF`

**RAM-04 — halfword load signed (LH)**
- After RAM-01: `daddr=0x100, size=2'b01, is_unsigned=0`
- Halfword = 0xBEEF → `rd = 0xFFFFBEEF`

**RAM-05 — halfword load unsigned (LHU)**
- After RAM-01: `daddr=0x100, size=2'b01, is_unsigned=1`
- `rd = 0x0000BEEF`

**RAM-06 — byte store (SB) does not disturb adjacent bytes**
- Write word `0xAABBCCDD` at `daddr=0x200`
- Write byte `0xFF` at `daddr=0x201`
- Read word at `daddr=0x200` → `0xAABBFFDD`

**RAM-07 — halfword store (SH)**
- `we=1, daddr=0x204, wd=0x1234, size=2'b01` → clock
- `daddr=0x204, size=2'b01, is_unsigned=1` → `rd=0x00001234`

**RAM-08 — write enable gating**
- `we=0, daddr=0x100, wd=0x12345678` → clock
- `daddr=0x100` → still `0xDEADBEEF`

**RAM-09 — multiple word addresses independent**
- Write `0x11111111` at `0x300`, `0x22222222` at `0x304`, `0x33333333` at `0x308`
- Read each → no aliasing

**RAM-10 — instruction port reads same backing array**
- Write word `0xDEADBEEF` via data port at `daddr=0x400` → clock
- Read via instruction port `iaddr=0x400` → `instr=0xDEADBEEF`

**RAM-11 — instruction port is combinational**
- Change `iaddr` without clocking → `instr` updates immediately

**RAM-12 — code region and stack region coexist**
- Write a word at `daddr=0x0000_0000` (code region) via data port
- Write a word at `daddr=0x0001_FFF0` (stack region)
- Read both back via data port → correct values, no aliasing

---

## 6. CPU Integration (`cpu.v`)

### Interface

```verilog
module cpu (
    input  wire        clk,
    input  wire        rst,

    // Debug initialization — active during rst=1, highest write priority
    input  wire        dbg_ram_we,
    input  wire [31:0] dbg_ram_addr,
    input  wire [31:0] dbg_ram_wd,
    input  wire        dbg_rf_we,
    input  wire [4:0]  dbg_rf_rd,
    input  wire [31:0] dbg_rf_wd,

    output wire [31:0] dbg_pc,
    output wire [31:0] dbg_x1,
    output wire [31:0] dbg_x2,
    output wire [31:0] dbg_x3,
    output wire [31:0] dbg_x4,
    output wire [31:0] dbg_x10,  // a0 — conventional return value
    output wire [31:0] dbg_instr,// current instruction word (ecall detection)
    output wire        dbg_rom_loading // high while ROM is streaming into RAM
);
```

Single-cycle. `rom_dump.v` (generated by `parse_rom.py`) is instantiated
inside `cpu.v` and auto-loads its words into RAM during reset — one word per
cycle while `rst=1`. Each word carries its actual ELF byte address via the
`rom_addr` output, so non-contiguous sections (`.text` + `.rodata`) land at
the correct locations. The testbench polls `dbg_rom_loading` to know when
loading is complete, then sets `sp=0x0001FFF0` via `dbg_rf_we` and releases
reset. Simulation halts when `dbg_instr == 32'h00000073` (`ecall`).

Normal instruction writes (`reg_write`, `mem_write`) are suppressed during
`rst=1`. Only debug-port writes (`dbg_rf_we`, `dbg_ram_we`) pass through.

### Source programs and build pipeline

Compiled test programs live under `codes/`:

```
codes/
├── src/                        # C++ source
│   ├── add.cpp                 # 3+2=5 → a0=5
│   ├── for_loop.cpp            # loop a=2+2+2 → a0=6
│   ├── t_if.cpp                # max(7,3)=7
│   ├── t_sub.cpp               # 20-7=13
│   ├── t_logic.cpp             # AND+OR+XOR=126
│   ├── t_shift.cpp             # SLL+SRL+SRA=32
│   ├── t_slt.cpp               # SLT×10+SLTU×5=15
│   ├── t_branch.cpp            # all 6 branch types → a0=63
│   ├── t_func.cpp              # function call add(5,7)=12
│   ├── t_lui.cpp               # LUI+ADDI → 0x000A0050=655440
│   ├── t_bytes.cpp             # SB/SH/LBU/LHU → 242
│   ├── t_rodata.cpp            # .rodata load: sum("Hello")=500
│   ├── pong_test.cpp           # player-controlled Pong; red miss screen + reset
│   └── calc.cpp                # interactive two-number adder (IO_SDK input + tb_io.cpp output)
├── instr/                      # objdump -d + objdump -s output (input to parse_rom.py)
├── builds/                     # ELF files (no linker script, qemu verification)
├── lds/                        # ELF files linked at 0x00000000 (for our CPU)
├── link.ld                     # .text + .rodata (with .srodata + ALIGN) + .data + .bss
├── build_riscv.sh              # compile → objdump -d + objdump -s → instr/<name>_instr.txt
└── build_and_run.sh            # compile + qemu (functional smoke-test)
```

**Build workflow for a new program:**

```bash
# 1. Compile and dump (run from core/, not codes/)
./codes/build_riscv.sh <name>
# produces codes/lds/<name>-rv32_ld.elf and codes/instr/<name>_instr.txt

# 2. Generate Verilog ROM
python3 parse_rom.py codes/instr/<name>_instr.txt

# 3. Run on CPU
./run.sh cpu tb_debug.cpp
```

---

### Test program A — arithmetic

```
addr  hex         assembly
0x00  00A00093    addi x1, x0, 10
0x04  00300113    addi x2, x0, 3
0x08  002081B3    add  x3, x1, x2
0x0C  40208233    sub  x4, x1, x2
```

Run 5 cycles. Assert `x1==10, x2==3, x3==13, x4==7`.

---

### Test program B — store and load

```
addr  hex         assembly
0x00  02A00093    addi x1, x0, 42
0x04  04000113    addi x2, x0, 64        # base address into data region
0x08  00112023    sw   x1, 0(x2)         # ram[64] = 42
0x0C  00012183    lw   x3, 0(x2)         # x3 = ram[64]
```

Run 5 cycles. Assert `x3 == 42`.

---

### Test program C — branch not taken

```
addr  hex         assembly
0x00  00100093    addi x1, x0, 1
0x04  00200113    addi x2, x0, 2
0x08  00208463    beq  x1, x2, +8        # x1!=x2 → not taken
0x0C  06300193    addi x3, x0, 99        # executes
0x10  00000213    addi x4, x0, 0
```

Run 6 cycles. Assert `x3 == 99`.

---

### Test program D — branch taken

```
addr  hex         assembly
0x00  00500093    addi x1, x0, 5
0x04  00500113    addi x2, x0, 5
0x08  00208463    beq  x1, x2, +8        # x1==x2 → taken → PC = 0x10
0x0C  03700193    addi x3, x0, 55        # skipped
0x10  04D00193    addi x3, x0, 77        # executes
```

Run 6 cycles. Assert `x3 == 77`.

---

### Test program E — JAL / JALR subroutine call

```
addr  hex         assembly
0x00  00A00093    addi x1, x0, 10
0x04  008002EF    jal  x5, +8            # x5=0x08 (PC+4), PC→0x0C
0x08  06300113    addi x2, x0, 99        # skipped
0x0C  02A00113    addi x2, x0, 42        # executes
0x10  00028067    jalr x0, 0(x5)         # PC = x5 = 0x08
0x08  06300113    addi x2, x0, 99        # executes on second pass
```

Run 8 cycles. Assert `x2 == 99` (last write wins).

---

### Test program F — LUI + ADDI (32-bit constant)

```
addr  hex         assembly
0x00  ABCDE0B7    lui  x1, 0xABCDE       # x1 = 0xABCDE000
0x04  123080093   — invalid: use 0x12308093 if upper bit of 0x123 is 0 (it is)
0x04  12308093    addi x1, x1, 0x123     # x1 = 0xABCDE123
```

Note: when the 12-bit immediate's MSB is 1, the LUI constant must be
incremented by 1 to compensate for sign-extension. Here 0x123 MSB=0, so no
adjustment needed.

Run 3 cycles. Assert `x1 == 0xABCDE123`.

---

### Test program G — compiled binary (`codes/instr/add_instr.txt`)

Real GCC-compiled RV32I binary linked at 0x0. Loaded into RAM automatically
by the `rom_dump` ROM loader during reset. Source: `codes/src/add.cpp`,
computes `3 + 2 = 5` using a stack frame and local variables.

**Precondition:** `sp = 0x0001FFF0` set via `dbg_rf_we` while `rst=1`.
Testbench holds `rst=1` for 32 cycles (margin for any program ≤ 32 words),
then sets `sp`, then releases reset.

**Binary (from `add_test/add_instr.txt`):**

```
addr  hex         assembly
0x00  FE010113    addi  sp, sp, -32         allocate 32-byte frame
0x04  00812E23    sw    s0, 28(sp)          save frame pointer
0x08  02010413    addi  s0, sp, 32          s0 = frame pointer
0x0C  00300793    li    a5, 3               a5 = 3
0x10  FEF42623    sw    a5, -20(s0)         local a = 3
0x14  00200793    li    a5, 2               a5 = 2
0x18  FEF42423    sw    a5, -24(s0)         local b = 2
0x1C  FEC42703    lw    a4, -20(s0)         a4 = a = 3
0x20  FE842783    lw    a5, -24(s0)         a5 = b = 2
0x24  00F707B3    add   a5, a4, a5          a5 = 3 + 2 = 5
0x28  FEF42223    sw    a5, -28(s0)         result = 5
0x2C  FE442503    lw    a0, -28(s0)         a0 = 5 (return value)
0x30  05D00893    li    a7, 93              syscall nr (ignored)
0x34  00000073    ecall                     halt
```

**Stack layout** (sp_init = 0x0001FFF0, s0 = sp_init):

```
s0      = 0x0001FFF0
s0 -  4 = 0x0001FFEC : (unused)
s0 -  8 = 0x0001FFE8 : saved s0     ← sw s0, 28(sp)  [sp = s0-32]
s0 - 20 = 0x0001FFDC : local a = 3  ← sw a5, -20(s0)
s0 - 24 = 0x0001FFD8 : local b = 2  ← sw a5, -24(s0)
s0 - 28 = 0x0001FFD4 : result = 5   ← sw a5, -28(s0)
sp      = 0x0001FFD0  (sp_init - 32)
```

**Assertion:** at ecall (PC=0x34): `a0 == 5`

**What this proves:** negative-immediate sign-extension (`addi sp,sp,-32`),
byte-addressed stores/loads to the stack region with negative s0-relative
offsets, and the full stack-frame sequence emitted by GCC -O0 for any
non-trivial function.

---

### Test program H — compiled binary (`codes/instr/for_loop_instr.txt`)

Real GCC-compiled RV32I binary with a `for` loop. Source: `codes/src/for_loop.cpp`.
Computes `a = 2`, then `a += 2` twice (loop `c = 0; c < 2; c++`), so `a = 6`.

**Binary (from `codes/instr/for_loop_instr.txt`, 19 words):**

```
addr  hex         assembly
0x00  FE010113    addi  sp, sp, -32
0x04  00812E23    sw    s0, 28(sp)
0x08  02010413    addi  s0, sp, 32
0x0C  00200793    li    a5, 2              a5 = 2
0x10  FEF42623    sw    a5, -20(s0)       local a = 2
0x14  FE042423    sw    zero, -24(s0)     local c = 0
0x18  01C0006F    j     0x34              goto loop test
0x1C  FEC42783    lw    a5, -20(s0)       loop body: a5 = a
0x20  00278793    addi  a5, a5, 2         a5 = a + 2
0x24  FEF42623    sw    a5, -20(s0)       a = a + 2
0x28  FE842783    lw    a5, -24(s0)       a5 = c
0x2C  00178793    addi  a5, a5, 1         a5 = c + 1
0x30  FEF42423    sw    a5, -24(s0)       c = c + 1
0x34  FE842703    lw    a4, -24(s0)       loop test: a4 = c
0x38  00100793    li    a5, 1             a5 = 1
0x3C  FEE7D0E3    bge   a5, a4, 0x1C     branch if 1 >= c (i.e. c <= 1)
0x40  FEC42503    lw    a0, -20(s0)       a0 = a (result)
0x44  05D00893    li    a7, 93
0x48  00000073    ecall
```

Loop trace: c=0 → a=4, c=1; c=1 → a=6, c=2; c=2 → `bge` not taken → exit.

**Assertion:** at ecall (PC=0x48): `a0 == 6`

**What this proves:** JAL (used for the initial `j` forward jump), BGE branch
with a loop-back negative offset, multiple store/load cycles over stack locals,
and correct loop termination. The 32-cycle reset window is required (19 words
+ sp-init tick = 20 ROM cycles total).

Generate and run:

```bash
python3 parse_rom.py codes/instr/for_loop_instr.txt
./run.sh cpu tb_debug.cpp
```

---

## Testbench conventions

- Each `.cpp` file is self-contained and runnable via `./run.sh <top> <tb>.cpp`
- Tests print `PASS` or `FAIL: <test-name> expected=0x... got=0x...`
- Exit code 0 on all pass, 1 on any failure
- Clock toggle: `clk=0 → eval → clk=1 → eval` per cycle

```cpp
void tick(Vcpu& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}
```

**Reset protocol for inline test programs (A–F):**

```cpp
top.rst = 1;
tick(top);                    // 1 cycle reset; ROM may write word 0 but
                              // test program overwrites it via dbg_ram_we

// load test program
top.dbg_ram_we = 1;
for (int i = 0; i < NWORDS; i++) {
    top.dbg_ram_addr = i * 4;
    top.dbg_ram_wd   = program[i];
    tick(top);
}
top.dbg_ram_we = 0;

top.rst = 0;                  // release reset
```

**Reset protocol for compiled programs (Programs G and H, and all `test.sh` programs):**

```cpp
top.rst = 1;
top.eval();
while (top.dbg_rom_loading) tick(top);  // wait for all ROM words to land

top.dbg_rf_we = 1; top.dbg_rf_rd = 2; top.dbg_rf_wd = 0x0001FFF0;
tick(top);                    // initialize sp (fires while rst=1)
top.dbg_rf_we = 0;

top.rst = 0;                  // release reset, CPU starts at PC=0
```

`dbg_rom_loading` is `rst & rom_valid` exported from `cpu.v`. Polling it
removes the need for a hard-coded cycle count and works for any program size.

**ecall detection:**
```cpp
while (top.dbg_instr != 0x00000073) {
    tick(top);
    if (cycle++ > MAX_CYCLES) { FAIL("timeout"); break; }
}
```

**ASSERT macro:**
```cpp
#define ASSERT(name, got, exp) do { \
    if ((got) != (exp)) { \
        printf("FAIL: %s  got=0x%08X  expected=0x%08X\n", name, got, exp); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)
```

- Global `int failures = 0;`; `return failures ? 1 : 0;` at end of main

**Debugger (`tb_debug.cpp`):**

Cycle-by-cycle instruction tracer. Reads all 32 registers via
`cpu->rootp->cpu__DOT__rf__DOT__regs[i]` (no `--public` flag needed).
Prints decoded assembly and register diffs each cycle. Stops at `ecall`.

```bash
./run.sh cpu tb_debug.cpp
```
