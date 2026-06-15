# RV32I Instruction Set — Implementation Reference

This document is a complete behavioral reference for every instruction the CPU
must implement. For each format family it covers: bit layout, the full opcode /
funct3 / funct7 table, and the exact semantics of every operation.

---

## How the CPU decodes an instruction

Every instruction is 32 bits wide. The first step is always:

```
opcode = instr[6:0]   → selects the FORMAT and broad operation class
funct3 = instr[14:12] → narrows within the class
funct7 = instr[31:25] → further disambiguation (R-type and shifts only)
```

The opcode alone tells you which format you are in, which determines:
- where rs1 / rs2 / rd live
- whether there is an immediate and which bits form it
- whether funct3 / funct7 are real control bits or part of the immediate

---

## R-type — register–register operations

### Bit layout

```
31      25 24   20 19   15 14  12 11    7 6      0
┌─────────┬───────┬───────┬──────┬───────┬────────┐
│ funct7  │  rs2  │  rs1  │funct3│  rd   │ opcode │
└─────────┴───────┴───────┴──────┴───────┴────────┘
  7 bits    5 bits  5 bits  3 bits  5 bits  7 bits
```

opcode = `0110011`

No immediate. Both source operands come from registers. Result written to `rd`.

### Instruction table

| Instruction | funct7    | funct3 | Operation |
|-------------|-----------|--------|-----------|
| ADD         | `0000000` | `000`  | `rd = rs1 + rs2` |
| SUB         | `0100000` | `000`  | `rd = rs1 - rs2` |
| AND         | `0000000` | `111`  | `rd = rs1 & rs2` |
| OR          | `0000000` | `110`  | `rd = rs1 \| rs2` |
| XOR         | `0000000` | `100`  | `rd = rs1 ^ rs2` |
| SLL         | `0000000` | `001`  | `rd = rs1 << rs2[4:0]` |
| SRL         | `0000000` | `101`  | `rd = rs1 >> rs2[4:0]` (logical) |
| SRA         | `0100000` | `101`  | `rd = rs1 >>> rs2[4:0]` (arithmetic) |
| SLT         | `0000000` | `010`  | `rd = (signed(rs1) < signed(rs2)) ? 1 : 0` |
| SLTU        | `0000000` | `011`  | `rd = (rs1 < rs2) ? 1 : 0` (unsigned) |

### Behavioral notes

**ADD / SUB**
Arithmetic is modulo 2³², overflow is silently discarded.
`0xFFFFFFFF + 1 = 0x00000000`. This is not an error.

**AND / OR / XOR**
Bitwise. No surprises. `XOR rd, rs1, rs1` is a common idiom to zero a register.

**SLL — Shift Left Logical**
Vacated bits on the right filled with 0.
Only the lower 5 bits of `rs2` are used as the shift amount (0–31).
`rs2 = 0xFFFFFFFF` is identical to `rs2 = 31`.

**SRL — Shift Right Logical**
Vacated bits on the left filled with 0.
Lower 5 bits of `rs2` used.

**SRA — Shift Right Arithmetic**
Vacated bits on the left filled with the **sign bit** (bit 31) of `rs1`.
`0x80000000 >> 1 = 0xC0000000`, not `0x40000000`.
In Verilog: `$signed(rs1) >>> rs2[4:0]`.

**SLT — Set Less Than (signed)**
Treats both operands as two's-complement signed integers.
`rs1 = 0xFFFFFFFF` = −1. −1 < 0 → result = 1.
`rs1 = 0x7FFFFFFF` = +2147483647. That > 0 → result = 0.

**SLTU — Set Less Than Unsigned**
Same as SLT but unsigned comparison.
`rs1 = 0xFFFFFFFF` is 4294967295 unsigned — the largest value.
`SLTU rd, 0xFFFFFFFF, 0` → result = 0 (not less than 0).

> **Decoder key:** ADD vs SUB and SRL vs SRA are distinguished **only** by
> `funct7[5]`. Everything else in funct7 must be 0 (for base RV32I).

---

## I-type — immediate operations

### Bit layout

```
31          20 19   15 14  12 11    7 6      0
┌─────────────┬───────┬──────┬───────┬────────┐
│  imm[11:0]  │  rs1  │funct3│  rd   │ opcode │
└─────────────┴───────┴──────┴───────┴────────┘
   12 bits      5 bits  3 bits  5 bits  7 bits
```

The 12-bit immediate is **always sign-extended** to 32 bits before use.
Range: −2048 to +2047.

Three distinct opcode families share this layout: ALU-immediate, loads, and JALR.

---

### I-type ALU — opcode `0010011`

One source register (`rs1`), one sign-extended immediate. Result to `rd`.
No `rs2` field.

| Instruction | funct3 | funct7 (if shift) | Operation |
|-------------|--------|-------------------|-----------|
| ADDI        | `000`  | —                 | `rd = rs1 + sext(imm)` |
| ANDI        | `111`  | —                 | `rd = rs1 & sext(imm)` |
| ORI         | `110`  | —                 | `rd = rs1 \| sext(imm)` |
| XORI        | `100`  | —                 | `rd = rs1 ^ sext(imm)` |
| SLTI        | `010`  | —                 | `rd = (signed(rs1) < signed(sext(imm))) ? 1 : 0` |
| SLTIU       | `011`  | —                 | `rd = (rs1 < sext(imm)) ? 1 : 0` (both unsigned after sext) |
| SLLI        | `001`  | `0000000`         | `rd = rs1 << imm[4:0]` |
| SRLI        | `101`  | `0000000`         | `rd = rs1 >> imm[4:0]` (logical) |
| SRAI        | `101`  | `0100000`         | `rd = rs1 >>> imm[4:0]` (arithmetic) |

### Behavioral notes

**ADDI**
The workhorse instruction. `ADDI rd, rs1, 0` copies a register (pseudo: `MV`).
`ADDI rd, x0, imm` loads a small constant (pseudo: `LI`).

**ANDI / ORI / XORI**
Same as R-type bitwise but second operand is the sign-extended immediate.
`XORI rd, rs1, -1` flips all bits (pseudo: `NOT`) because −1 = `0xFFFFFFFF`.

**SLTI / SLTIU**
Immediate is sign-extended first, then compared.
For SLTIU the comparison is unsigned but the immediate is still sign-extended:
`SLTIU rd, rs1, 1` tests if `rs1 == 0` (pseudo: `SEQZ`), because the only
unsigned value less than 1 is 0.

**SLLI / SRLI / SRAI**
For shifts, the immediate field is reinterpreted:
```
imm[11:5] = funct7  →  must be 0000000 (SLLI/SRLI) or 0100000 (SRAI)
imm[4:0]  = shamt   →  shift amount, 0–31
```
The `rs2` field does not exist here. `funct7` distinguishes logical vs arithmetic
in the same way as R-type SRL vs SRA.

---

### I-type Loads — opcode `0000011`

Address = `rs1 + sext(imm)`. Data read from memory, written to `rd`.

| Instruction | funct3 | Operation |
|-------------|--------|-----------|
| LB          | `000`  | `rd = sext(mem[addr][7:0])`   — load byte, sign-extend |
| LH          | `001`  | `rd = sext(mem[addr][15:0])`  — load halfword, sign-extend |
| LW          | `010`  | `rd = mem[addr][31:0]`        — load word |
| LBU         | `100`  | `rd = zext(mem[addr][7:0])`   — load byte, zero-extend |
| LHU         | `101`  | `rd = zext(mem[addr][15:0])`  — load halfword, zero-extend |

### Behavioral notes

**Address computation**
The ALU computes `rs1 + sext(imm)` — identical to ADDI. The result is a
byte address into memory.

**Sign extension vs zero extension**
`LB` reads one byte and fills the upper 24 bits with the byte's bit 7.
`LBU` reads one byte and fills the upper 24 bits with 0.
Example: reading byte `0xEF`:
- `LB`  → `0xFFFFFFEF` (bit 7 of 0xEF = 1, so extended with 1s)
- `LBU` → `0x000000EF`

**Endianness**
RISC-V is **little-endian**: the byte at the lowest address is the least
significant byte of a word.
Word `0xDEADBEEF` stored at address 0:
```
addr 0 → 0xEF
addr 1 → 0xBE
addr 2 → 0xAD
addr 3 → 0xDE
```

---

### I-type Jump — JALR — opcode `1100111`, funct3 `000`

```
rd  = PC + 4
PC  = (rs1 + sext(imm)) & ~1    ← lowest bit forced to 0
```

### Behavioral notes

`rd` receives the return address (next sequential PC). The target is `rs1 +
imm` with bit 0 cleared — this aligns the jump target to a 2-byte boundary
(required by the ISA, in case the C extension is present).

Common use:
- `JALR x0, 0(ra)` — return from function (`ra` = x1 holds return address)
- `JALR ra, 0(t1)` — call function pointer in `t1`

---

## S-type — stores

### Bit layout

```
31      25 24   20 19   15 14  12 11    7 6      0
┌─────────┬───────┬───────┬──────┬───────┬────────┐
│imm[11:5]│  rs2  │  rs1  │funct3│imm[4:0]│opcode │
└─────────┴───────┴───────┴──────┴───────┴────────┘
```

opcode = `0100011`

The immediate is **split**: upper 7 bits at [31:25], lower 5 bits at [11:7].
`imm_gen` must reassemble them: `{instr[31:25], instr[11:7]}`.
Address = `rs1 + sext(imm)`. Data source = `rs2`.

### Instruction table

| Instruction | funct3 | Operation |
|-------------|--------|-----------|
| SB          | `000`  | `mem[rs1+imm][7:0]  = rs2[7:0]` |
| SH          | `001`  | `mem[rs1+imm][15:0] = rs2[15:0]` |
| SW          | `010`  | `mem[rs1+imm][31:0] = rs2` |

### Behavioral notes

There is no `rd` field in S-type. Bits [11:7] are the lower immediate, not a
destination register.

**Byte store** writes only 1 byte, leaving the other 3 bytes of the word
unchanged. Hardware uses byte-enable signals to do this.

**Halfword store** writes 2 consecutive bytes, little-endian.

`SW` is the most common. `SB` and `SH` appear when writing to packed structs
or hardware registers.

---

## B-type — conditional branches

### Bit layout

```
31 30      25 24   20 19   15 14  12 11    8 7 6      0
┌──┬─────────┬───────┬───────┬──────┬───────┬──┬────────┐
│12│ imm[10:5]│  rs2  │  rs1  │funct3│imm[4:1]│11│opcode │
└──┴─────────┴───────┴───────┴──────┴───────┴──┴────────┘
```

opcode = `1100011`

The immediate encodes a **PC-relative byte offset**. Bit 0 is always 0 (not
stored). Bits are scrambled to keep the sign bit at position 31.

`imm_gen` reassembly:
```
imm[12]   = instr[31]
imm[11]   = instr[7]
imm[10:5] = instr[30:25]
imm[4:1]  = instr[11:8]
imm[0]    = 0  (implicit)
```

If branch taken: `PC = PC + sext(imm)`. Range: −4096 to +4094 bytes.

### Instruction table

| Instruction | funct3 | Condition | ALU check |
|-------------|--------|-----------|-----------|
| BEQ         | `000`  | `rs1 == rs2` | SUB → zero=1 |
| BNE         | `001`  | `rs1 != rs2` | SUB → zero=0 |
| BLT         | `100`  | `signed(rs1) < signed(rs2)` | SLT → result=1 |
| BGE         | `101`  | `signed(rs1) >= signed(rs2)` | SLT → result=0 |
| BLTU        | `110`  | `rs1 < rs2` (unsigned) | SLTU → result=1 |
| BGEU        | `111`  | `rs1 >= rs2` (unsigned) | SLTU → result=0 |

### Behavioral notes

**No BGT or BLE in RV32I.**
These are pseudo-instructions: `BGT rs1, rs2` assembles as `BLT rs2, rs1`
(swap operands). The assembler handles this; the hardware only sees BLT.

**How branches map to the ALU:**
- BEQ/BNE: ALU does SUB, checks `zero` flag
- BLT/BGE: ALU does SLT, checks if result is 1 or 0
- BLTU/BGEU: ALU does SLTU, checks if result is 1 or 0

**The offset is from the branch instruction's own PC**, not PC+4.
`BEQ x1, x2, +8` at address 0x08 jumps to 0x08+8 = 0x10.

---

## U-type — upper immediate

### Bit layout

```
31                  12 11    7 6      0
┌────────────────────┬────────┬────────┐
│     imm[31:12]     │   rd   │ opcode │
└────────────────────┴────────┴────────┘
       20 bits          5 bits  7 bits
```

No `rs1`, no `rs2`, no `funct3`. The entire upper 20 bits are the immediate.

### Instruction table

| Instruction | opcode    | Operation |
|-------------|-----------|-----------|
| LUI         | `0110111` | `rd = {imm[31:12], 12'b0}` |
| AUIPC       | `0010111` | `rd = PC + {imm[31:12], 12'b0}` |

### Behavioral notes

**LUI — Load Upper Immediate**
Places a 20-bit value into bits [31:12] of `rd`, zeroing bits [11:0].
Used with ADDI to load any 32-bit constant:
```asm
lui  rd, 0xABCDE        # rd = 0xABCDE000
addi rd, rd, 0x123      # rd = 0xABCDE123
```
Caveat: ADDI sign-extends its 12-bit immediate. If the lower 12 bits have
bit 11 = 1 (i.e. value ≥ 0x800), ADDI subtracts from the upper part.
Compensate by adding 1 to the LUI value:
```asm
lui  rd, 0xABCDF        # +1 because lower 0xF23 has bit11=1
addi rd, rd, -0x0DD     # 0xF23 sign-extended = -0x0DD
# result: 0xABCDE + 0xF23 = 0xABCDF000 - 0xDD = 0xABCDEF23 ✓
```

**AUIPC — Add Upper Immediate to PC**
Adds the shifted immediate to the **current PC**. Used to compute
PC-relative addresses, typically in pairs with JALR for long-range calls:
```asm
auipc ra, %hi(target)    # ra = PC + upper part of offset
jalr  ra, %lo(target)(ra) # ra = PC+4; jump to full target
```
Without AUIPC, a function call farther than ±1MB (JAL range) would be
impossible without an intermediate register.

---

## J-type — unconditional jump

### Bit layout

```
31 30       21 20 19      12 11    7 6      0
┌──┬──────────┬──┬──────────┬────────┬────────┐
│20│ imm[10:1] │11│ imm[19:12]│   rd   │ opcode │
└──┴──────────┴──┴──────────┴────────┴────────┘
```

opcode = `1101111`

Bit 0 is implicit 0. Bits are scrambled (same principle as B-type: sign at 31).

`imm_gen` reassembly:
```
imm[20]    = instr[31]
imm[19:12] = instr[19:12]
imm[11]    = instr[20]
imm[10:1]  = instr[30:21]
imm[0]     = 0
```

Range: ±1 MB (−1048576 to +1048574 bytes, in steps of 2).

### Instruction table

| Instruction | opcode    | Operation |
|-------------|-----------|-----------|
| JAL         | `1101111` | `rd = PC + 4` then `PC = PC + sext(imm)` |

### Behavioral notes

**JAL — Jump and Link**
Saves the return address (PC+4) in `rd`, then jumps to `PC + imm`.

Common uses:
- `JAL ra, offset` — call a function, `ra` (x1) = return address
- `JAL x0, offset` — unconditional jump with no link (pseudo: `J`)

The offset is PC-relative. If `_start` is at 0x00 and `func` is at 0x10:
```asm
# at 0x00:
jal ra, func      # ra = 0x04, PC → 0x10
```

---

## SYSTEM — ecall / ebreak

### Bit layout

Same as I-type. opcode = `1110011`, funct3 = `000`.

| Instruction | imm[11:0]    | Behavior |
|-------------|-------------|----------|
| ECALL       | `000000000000` | Transfer control to execution environment |
| EBREAK      | `000000000001` | Transfer control to debugger |

### Behavioral notes

In a bare-metal simulation, **ECALL is treated as a halt signal**. The
testbench detects `instr == 32'h00000073` and stops the simulation. No
privilege mode or OS involvement needed.

Register `a7` (x17) conventionally holds the "syscall number" when running
under a Linux ABI, but for our purposes it is ignored.

---

## Complete opcode map

```
opcode    format  class
───────────────────────────────────────────
0110011   R       register ALU
0010011   I       immediate ALU
0000011   I       loads
1100111   I       JALR
0100011   S       stores
1100011   B       branches
0110111   U       LUI
0010111   U       AUIPC
1101111   J       JAL
1110011   I       SYSTEM (ecall/ebreak)
```

---

## What each stage of the CPU does with each field

```
Stage        Fields consumed
──────────────────────────────────────────────────────────
Fetch        PC → instruction memory → instr[31:0]
Decode       instr[6:0]   → opcode → format, control signals
             instr[14:12] → funct3 → ALU op / branch type / mem size
             instr[31:25] → funct7 → ALU op disambiguation
             instr[19:15] → rs1 address → register file port A
             instr[24:20] → rs2 address → register file port B
             instr[11:7]  → rd address  → register file write port
             instr[31:0]  → imm_gen     → sign-extended immediate
Execute      ALU: (rs1 or PC) op (rs2 or imm) → result, zero flag
Memory       address = ALU result
             on load:  data = ram[address]
             on store: ram[address] = rs2
Writeback    rd ← ALU result / load data / PC+4
             PC ← PC+4 / branch target / jump target
```
