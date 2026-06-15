# How RISC-V Instructions Become Binary

A RISC-V instruction is a 32-bit word. Assembly instructions such as:

```asm
add x5, x6, x7
```

are translated by the assembler into a binary encoding.

For RV32I, every normal instruction is 32 bits long.

---

# R-Type Encoding

Used by register-register ALU instructions:

```asm
add x5, x6, x7
sub x5, x6, x7
and x5, x6, x7
or  x5, x6, x7
```

Format:

```text
31      25 24   20 19   15 14 12 11    7 6      0
+---------+-------+-------+-----+--------+--------+
| funct7  | rs2   | rs1   | f3  | rd     | opcode |
+---------+-------+-------+-----+--------+--------+
```

Example:

```asm
add x5, x6, x7
```

Fields:

```text
funct7 = 0000000
rs2    = 00111   // x7
rs1    = 00110   // x6
funct3 = 000
rd     = 00101   // x5
opcode = 0110011
```

Full binary:

```text
0000000 00111 00110 000 00101 0110011
```

Hex:

```text
0x007302b3
```

So:

```asm
add x5, x6, x7
```

becomes:

```verilog
32'h007302b3
```

---

# I-Type Encoding

Used by immediate instructions and loads:

```asm
addi x5, x6, 10
lw   x5, 0(x6)
jalr x1, 0(x5)
```

Format:

```text
31          20 19   15 14 12 11    7 6      0
+-------------+-------+-----+--------+--------+
| imm[11:0]   | rs1   | f3  | rd     | opcode |
+-------------+-------+-----+--------+--------+
```

Example:

```asm
addi x5, x6, 10
```

Fields:

```text
imm    = 000000001010   // 10
rs1    = 00110          // x6
funct3 = 000
rd     = 00101          // x5
opcode = 0010011
```

Full binary:

```text
000000001010 00110 000 00101 0010011
```

Hex:

```text
0x00a30293
```

So:

```asm
addi x5, x6, 10
```

becomes:

```verilog
32'h00a30293
```

---

# S-Type Encoding

Used by stores:

```asm
sw x5, 8(x6)
```

Format:

```text
31      25 24   20 19   15 14 12 11     7 6      0
+---------+-------+-------+-----+---------+--------+
| imm[11:5]| rs2  | rs1   | f3  | imm[4:0]| opcode |
+---------+-------+-------+-----+---------+--------+
```

Example:

```asm
sw x5, 8(x6)
```

Meaning:

```text
memory[x6 + 8] = x5
```

Fields:

```text
imm    = 000000001000   // 8
rs2    = 00101          // x5
rs1    = 00110          // x6
funct3 = 010            // word store
opcode = 0100011
```

Immediate split:

```text
imm[11:5] = 0000000
imm[4:0]  = 01000
```

Full binary:

```text
0000000 00101 00110 010 01000 0100011
```

Hex:

```text
0x00532423
```

---

# B-Type Encoding

Used by branches:

```asm
beq x5, x6, label
```

Format:

```text
31     30:25 24   20 19   15 14 12 11:8  7 6      0
+--------+------+-------+-------+-----+-----+--------+--------+
|imm[12] |imm[10:5]|rs2 | rs1   | f3  |imm[4:1]|imm[11]|opcode|
+--------+------+-------+-------+-----+-----+--------+--------+
```

Branches use a signed PC-relative offset.

Important detail:

```text
The branch offset is multiplied by 2.
```

So the lowest bit is not stored.

Example:

```asm
beq x5, x6, +8
```

means:

```text
if x5 == x6:
    pc = pc + 8
```

Fields:

```text
rs1    = 00101   // x5
rs2    = 00110   // x6
funct3 = 000     // BEQ
opcode = 1100011
```

---

# U-Type Encoding

Used by:

```asm
lui   x5, imm
auipc x5, imm
```

Format:

```text
31                 12 11    7 6      0
+--------------------+--------+--------+
| imm[31:12]         | rd     | opcode |
+--------------------+--------+--------+
```

Example:

```asm
lui x5, 0x12345
```

Fields:

```text
imm    = 0x12345
rd     = x5
opcode = 0110111
```

Hex:

```text
0x123452b7
```

---

# J-Type Encoding

Used by:

```asm
jal x1, offset
```

Format:

```text
31     30:21 20     19:12 11    7 6      0
+--------+------+--------+------+--------+--------+
|imm[20] |imm[10:1]|imm[11]|imm[19:12]|rd| opcode |
+--------+------+--------+------+--------+--------+
```

`jal` stores the return address in `rd`:

```text
rd = pc + 4
pc = pc + offset
```

Example:

```asm
jal x1, function
```

usually means:

```text
x1 = return address
jump to function
```

In RISC-V, `x1` is conventionally the return address register `ra`.

---

# Common RV32I Opcode Values

```text
0110011  R-type ALU      ADD, SUB, AND, OR, XOR
0010011  I-type ALU      ADDI, ANDI, ORI, XORI
0000011  Loads           LB, LH, LW, LBU, LHU
0100011  Stores          SB, SH, SW
1100011  Branches        BEQ, BNE, BLT, BGE
1101111  JAL
1100111  JALR
0110111  LUI
0010111  AUIPC
```

---

# Example Program

Assembly:

```asm
addi x1, x0, 10
addi x2, x0, 3
add  x3, x1, x2
sub  x4, x1, x2
```

Machine code:

```text
0x00a00093
0x00300113
0x002081b3
0x40208233
```

In Verilog ROM:

```verilog
initial begin
    rom[0] = 32'h00a00093; // addi x1, x0, 10
    rom[1] = 32'h00300113; // addi x2, x0, 3
    rom[2] = 32'h002081b3; // add  x3, x1, x2
    rom[3] = 32'h40208233; // sub  x4, x1, x2
end
```

Expected result:

```text
x1 = 10
x2 = 3
x3 = 13
x4 = 7
```

---

# Decoder Implication

Your CPU should not decode text like:

```asm
add x3, x1, x2
```

It should decode binary fields:

```verilog
assign opcode = instr[6:0];
assign rd     = instr[11:7];
assign funct3 = instr[14:12];
assign rs1    = instr[19:15];
assign rs2    = instr[24:20];
assign funct7 = instr[31:25];
```

Then:

```verilog
case (opcode)
    7'b0110011: begin
        case ({funct7, funct3})
            {7'b0000000, 3'b000}: alu_op = ALU_ADD;
            {7'b0100000, 3'b000}: alu_op = ALU_SUB;
            {7'b0000000, 3'b111}: alu_op = ALU_AND;
            {7'b0000000, 3'b110}: alu_op = ALU_OR;
        endcase
    end

    7'b0010011: begin
        case (funct3)
            3'b000: alu_op = ALU_ADD; // ADDI
            3'b111: alu_op = ALU_AND; // ANDI
            3'b110: alu_op = ALU_OR;  // ORI
        endcase
    end
endcase
```

So the assembler translates assembly into binary, and your CPU only executes the binary.

