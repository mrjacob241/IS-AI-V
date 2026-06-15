#include "Vcontrol.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-45s  got=%2u  expected=%2u\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while(0)

// ALU ops
static const uint8_t ALU_ADD  = 0x0;
static const uint8_t ALU_SUB  = 0x1;
static const uint8_t ALU_AND  = 0x2;
static const uint8_t ALU_OR   = 0x3;
static const uint8_t ALU_XOR  = 0x4;
static const uint8_t ALU_SLL  = 0x5;
static const uint8_t ALU_SRL  = 0x6;
static const uint8_t ALU_SRA  = 0x7;
static const uint8_t ALU_SLT  = 0x8;
static const uint8_t ALU_SLTU = 0x9;

// mem_to_reg
static const uint8_t WB_ALU = 0x0;
static const uint8_t WB_MEM = 0x1;
static const uint8_t WB_PC4 = 0x2;

struct Expect {
    uint8_t alu_src;
    uint8_t alu_op;
    uint8_t reg_write;
    uint8_t mem_read;
    uint8_t mem_write;
    uint8_t mem_to_reg;
    uint8_t branch;
    uint8_t branch_inv;
    uint8_t jump;
    uint8_t jalr;
    uint8_t pc_to_alu;
    uint8_t mem_size;
    uint8_t mem_unsigned;
};

void check(Vcontrol& top, const char* label,
           uint8_t opcode, uint8_t funct3, uint8_t funct7,
           const Expect& e) {
    top.opcode = opcode;
    top.funct3 = funct3;
    top.funct7 = funct7;
    top.eval();

    char buf[80];
#define CHK(field) \
    snprintf(buf, sizeof(buf), "%s." #field, label); \
    ASSERT(buf, top.field, e.field)

    CHK(alu_src);
    CHK(alu_op);
    CHK(reg_write);
    CHK(mem_read);
    CHK(mem_write);
    CHK(mem_to_reg);
    CHK(branch);
    CHK(branch_inv);
    CHK(jump);
    CHK(jalr);
    CHK(pc_to_alu);
    CHK(mem_size);
    CHK(mem_unsigned);
#undef CHK
}

int main() {
    Vcontrol top;

    // ---------------------------------------------------------------
    // CTRL-01/02/03  R-type (opcode=0x33)
    // ---------------------------------------------------------------
    struct { uint8_t f7; uint8_t f3; uint8_t expected_op; const char* name; } r_ops[] = {
        { 0x00, 0b000, ALU_ADD,  "CTRL-03 R-type ADD"  },
        { 0x20, 0b000, ALU_SUB,  "CTRL-03 R-type SUB"  },
        { 0x00, 0b111, ALU_AND,  "CTRL-03 R-type AND"  },
        { 0x00, 0b110, ALU_OR,   "CTRL-03 R-type OR"   },
        { 0x00, 0b100, ALU_XOR,  "CTRL-03 R-type XOR"  },
        { 0x00, 0b001, ALU_SLL,  "CTRL-03 R-type SLL"  },
        { 0x00, 0b101, ALU_SRL,  "CTRL-03 R-type SRL"  },
        { 0x20, 0b101, ALU_SRA,  "CTRL-03 R-type SRA"  },
        { 0x00, 0b010, ALU_SLT,  "CTRL-03 R-type SLT"  },
        { 0x00, 0b011, ALU_SLTU, "CTRL-03 R-type SLTU" },
    };
    for (auto& op : r_ops) {
        Expect e = {0, op.expected_op, 1, 0, 0, WB_ALU, 0, 0, 0, 0, 0, 0b10, 0};
        check(top, op.name, 0x33, op.f3, op.f7, e);
    }

    // ---------------------------------------------------------------
    // CTRL-04/05  I-type ALU (opcode=0x13)
    // ---------------------------------------------------------------
    struct { uint8_t f7; uint8_t f3; uint8_t expected_op; const char* name; } i_ops[] = {
        { 0x00, 0b000, ALU_ADD,  "CTRL-04 ADDI"  },
        { 0x00, 0b111, ALU_AND,  "CTRL-05 ANDI"  },
        { 0x00, 0b110, ALU_OR,   "CTRL-05 ORI"   },
        { 0x00, 0b100, ALU_XOR,  "CTRL-05 XORI"  },
        { 0x00, 0b010, ALU_SLT,  "CTRL-05 SLTI"  },
        { 0x00, 0b011, ALU_SLTU, "CTRL-05 SLTIU" },
        { 0x00, 0b001, ALU_SLL,  "CTRL-05 SLLI"  },
        { 0x00, 0b101, ALU_SRL,  "CTRL-05 SRLI"  },
        { 0x20, 0b101, ALU_SRA,  "CTRL-05 SRAI"  },
    };
    for (auto& op : i_ops) {
        Expect e = {1, op.expected_op, 1, 0, 0, WB_ALU, 0, 0, 0, 0, 0, 0b10, 0};
        check(top, op.name, 0x13, op.f3, op.f7, e);
    }

    // ---------------------------------------------------------------
    // CTRL-06/07  Loads (opcode=0x03)
    // ---------------------------------------------------------------
    struct { uint8_t f3; uint8_t size; uint8_t uns; const char* name; } loads[] = {
        { 0b000, 0b00, 0, "CTRL-06 LB"  },
        { 0b001, 0b01, 0, "CTRL-07 LH"  },
        { 0b010, 0b10, 0, "CTRL-06 LW"  },
        { 0b100, 0b00, 1, "CTRL-07 LBU" },
        { 0b101, 0b01, 1, "CTRL-07 LHU" },
    };
    for (auto& ld : loads) {
        Expect e = {1, ALU_ADD, 1, 1, 0, WB_MEM, 0, 0, 0, 0, 0, ld.size, ld.uns};
        check(top, ld.name, 0x03, ld.f3, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-08  Stores (opcode=0x23)
    // ---------------------------------------------------------------
    struct { uint8_t f3; uint8_t size; const char* name; } stores[] = {
        { 0b000, 0b00, "CTRL-08 SB" },
        { 0b001, 0b01, "CTRL-08 SH" },
        { 0b010, 0b10, "CTRL-08 SW" },
    };
    for (auto& st : stores) {
        Expect e = {1, ALU_ADD, 0, 0, 1, WB_ALU, 0, 0, 0, 0, 0, st.size, 0};
        check(top, st.name, 0x23, st.f3, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-09/10  Branches (opcode=0x63)
    // ---------------------------------------------------------------
    struct { uint8_t f3; uint8_t op; uint8_t inv; const char* name; } branches[] = {
        { 0b000, ALU_SUB,  0, "CTRL-09 BEQ"  },
        { 0b001, ALU_SUB,  1, "CTRL-10 BNE"  },
        { 0b100, ALU_SLT,  0, "CTRL-10 BLT"  },
        { 0b101, ALU_SLT,  1, "CTRL-10 BGE"  },
        { 0b110, ALU_SLTU, 0, "CTRL-10 BLTU" },
        { 0b111, ALU_SLTU, 1, "CTRL-10 BGEU" },
    };
    for (auto& br : branches) {
        Expect e = {0, br.op, 0, 0, 0, WB_ALU, 1, br.inv, 0, 0, 0, 0b10, 0};
        check(top, br.name, 0x63, br.f3, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-11  JAL (opcode=0x6F)
    // ---------------------------------------------------------------
    {
        Expect e = {0, ALU_ADD, 1, 0, 0, WB_PC4, 0, 0, 1, 0, 0, 0b10, 0};
        check(top, "CTRL-11 JAL", 0x6F, 0x00, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-12  JALR (opcode=0x67)
    // ---------------------------------------------------------------
    {
        Expect e = {1, ALU_ADD, 1, 0, 0, WB_PC4, 0, 0, 1, 1, 0, 0b10, 0};
        check(top, "CTRL-12 JALR", 0x67, 0x00, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-13  LUI (opcode=0x37)
    // ---------------------------------------------------------------
    {
        Expect e = {1, ALU_ADD, 1, 0, 0, WB_ALU, 0, 0, 0, 0, 0, 0b10, 0};
        check(top, "CTRL-13 LUI", 0x37, 0x00, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-14  AUIPC (opcode=0x17)
    // ---------------------------------------------------------------
    {
        Expect e = {1, ALU_ADD, 1, 0, 0, WB_ALU, 0, 0, 0, 0, 1, 0b10, 0};
        check(top, "CTRL-14 AUIPC", 0x17, 0x00, 0x00, e);
    }

    // ---------------------------------------------------------------
    // CTRL-15  ECALL — all control signals at safe defaults
    // ---------------------------------------------------------------
    {
        Expect e = {0, ALU_ADD, 0, 0, 0, WB_ALU, 0, 0, 0, 0, 0, 0b10, 0};
        check(top, "CTRL-15 ECALL (safe defaults)", 0x73, 0x00, 0x00, e);
    }

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
