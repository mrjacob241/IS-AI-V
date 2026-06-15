#include "Vcpu.h"
#include "Vcpu___024root.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

// -------------------------------------------------------------------------
// ABI register names
// -------------------------------------------------------------------------
static const char* RNAME[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0",  "s1","a0","a1","a2","a3","a4","a5",
    "a6",  "a7","s2","s3","s4","s5","s6","s7",
    "s8",  "s9","s10","s11","t3","t4","t5","t6"
};

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------
static void tick(Vcpu& top) {
    top.clk = 0; top.eval();
    top.clk = 1; top.eval();
}

static uint32_t get_reg(Vcpu& top, int i) {
    return (uint32_t)top.rootp->cpu__DOT__rf__DOT__regs[i];
}

// Safe sign-extension: extend 'bits'-wide value to 32 bits
static int32_t sext(uint32_t val, int bits) {
    val &= (1u << bits) - 1u;
    if (val & (1u << (bits - 1)))
        val |= ~((1u << bits) - 1u);
    return (int32_t)val;
}

// -------------------------------------------------------------------------
// Mini RV32I disassembler
// -------------------------------------------------------------------------
static void decode(uint32_t instr, uint32_t pc, char* buf, size_t sz) {
    uint32_t op  = instr & 0x7F;
    uint32_t rd  = (instr >>  7) & 0x1F;
    uint32_t f3  = (instr >> 12) & 0x07;
    uint32_t rs1 = (instr >> 15) & 0x1F;
    uint32_t rs2 = (instr >> 20) & 0x1F;
    uint32_t f7  = (instr >> 25) & 0x7F;

    int32_t imm_i = sext(instr >> 20, 12);
    int32_t imm_s = sext(((instr >> 25) << 5) | ((instr >> 7) & 0x1F), 12);
    int32_t imm_b = sext(((instr >> 31) << 12) | (((instr >> 7) & 1) << 11) |
                         (((instr >> 25) & 0x3F) << 5) | (((instr >> 8) & 0xF) << 1), 13);
    int32_t imm_u = (int32_t)(instr & 0xFFFFF000);
    int32_t imm_j = sext(((instr >> 31) << 20) | (((instr >> 12) & 0xFF) << 12) |
                         (((instr >> 20) & 1) << 11) | (((instr >> 21) & 0x3FF) << 1), 21);

    switch (op) {

    case 0x33: { // R-type
        const char* mn = "???";
        if      (f3==0 && f7==0x00) mn = "add";
        else if (f3==0 && f7==0x20) mn = "sub";
        else if (f3==1 && f7==0x00) mn = "sll";
        else if (f3==2 && f7==0x00) mn = "slt";
        else if (f3==3 && f7==0x00) mn = "sltu";
        else if (f3==4 && f7==0x00) mn = "xor";
        else if (f3==5 && f7==0x00) mn = "srl";
        else if (f3==5 && f7==0x20) mn = "sra";
        else if (f3==6 && f7==0x00) mn = "or";
        else if (f3==7 && f7==0x00) mn = "and";
        snprintf(buf, sz, "%-6s %s, %s, %s", mn, RNAME[rd], RNAME[rs1], RNAME[rs2]);
        break;
    }

    case 0x13: { // I-type ALU
        const char* mn = "???";
        if      (f3==0) mn = "addi";
        else if (f3==1) mn = "slli";
        else if (f3==2) mn = "slti";
        else if (f3==3) mn = "sltiu";
        else if (f3==4) mn = "xori";
        else if (f3==5 && f7==0x00) mn = "srli";
        else if (f3==5 && f7==0x20) mn = "srai";
        else if (f3==6) mn = "ori";
        else if (f3==7) mn = "andi";
        // shifts use shamt (rs2 field), not full imm
        if (f3 == 1 || f3 == 5)
            snprintf(buf, sz, "%-6s %s, %s, %d", mn, RNAME[rd], RNAME[rs1], (int)rs2);
        else
            snprintf(buf, sz, "%-6s %s, %s, %d", mn, RNAME[rd], RNAME[rs1], imm_i);
        break;
    }

    case 0x03: { // Loads
        const char* loads[] = {"lb","lh","lw","???","lbu","lhu"};
        snprintf(buf, sz, "%-6s %s, %d(%s)", f3 < 6 ? loads[f3] : "???",
                 RNAME[rd], imm_i, RNAME[rs1]);
        break;
    }

    case 0x23: { // Stores
        const char* stores[] = {"sb","sh","sw"};
        snprintf(buf, sz, "%-6s %s, %d(%s)", f3 < 3 ? stores[f3] : "???",
                 RNAME[rs2], imm_s, RNAME[rs1]);
        break;
    }

    case 0x63: { // Branches
        const char* brs[] = {"beq","bne","???","???","blt","bge","bltu","bgeu"};
        snprintf(buf, sz, "%-6s %s, %s, 0x%04X (%+d)",
                 brs[f3], RNAME[rs1], RNAME[rs2], pc + imm_b, imm_b);
        break;
    }

    case 0x6F: // JAL
        snprintf(buf, sz, "%-6s %s, 0x%04X (%+d)", "jal", RNAME[rd], pc + imm_j, imm_j);
        break;

    case 0x67: // JALR
        snprintf(buf, sz, "%-6s %s, %d(%s)", "jalr", RNAME[rd], imm_i, RNAME[rs1]);
        break;

    case 0x37: // LUI
        snprintf(buf, sz, "%-6s %s, 0x%05X", "lui", RNAME[rd], (uint32_t)imm_u >> 12);
        break;

    case 0x17: // AUIPC
        snprintf(buf, sz, "%-6s %s, 0x%05X", "auipc", RNAME[rd], (uint32_t)imm_u >> 12);
        break;

    case 0x73:
        snprintf(buf, sz, "ecall");
        break;

    default:
        snprintf(buf, sz, "??? (0x%08X)", instr);
        break;
    }
}

// -------------------------------------------------------------------------
// Main
// -------------------------------------------------------------------------
int main() {
    Vcpu top;

    top.clk        = 0;
    top.rst        = 1;
    top.dbg_ram_we = 0; top.dbg_ram_addr = 0; top.dbg_ram_wd = 0;
    top.dbg_rf_we  = 0; top.dbg_rf_rd   = 0; top.dbg_rf_wd  = 0;
    top.eval();

    // --- ROM loads automatically via rom_dump during rst=1 ---
    // Extra cycles past NWORDS are safe: rom_valid goes low and no writes occur.
    printf("Loading ROM...\n");
    top.eval();
    while (top.dbg_rom_loading) tick(top);

    // --- Pre-initialise sp = 0x0001FFF0 ---
    top.dbg_rf_we = 1;
    top.dbg_rf_rd = 2;
    top.dbg_rf_wd = 0x0001FFF0;
    tick(top);
    top.dbg_rf_we = 0;

    // --- Release reset ---
    top.rst = 0;
    top.eval();

    // Snapshot register state after reset
    uint32_t prev[32] = {};
    for (int i = 0; i < 32; i++) prev[i] = get_reg(top, i);

    // --- Execution trace ---
    printf("\n");
    printf("Cycle  PC      Instr       Assembly                              Changes\n");
    printf("%s\n", std::string(100, '-').c_str());

    static const int MAX_CYCLES = 200;

    for (int cycle = 0; cycle < MAX_CYCLES; cycle++) {
        uint32_t pc    = (uint32_t)top.dbg_pc;
        uint32_t instr = (uint32_t)top.dbg_instr;

        char asm_buf[64];
        decode(instr, pc, asm_buf, sizeof(asm_buf));

        bool is_ecall = (instr == 0x00000073);

        // Print instruction
        printf("[%3d]  0x%04X  %08X  %-38s", cycle, pc, instr, asm_buf);

        // Clock
        tick(top);

        // Print changed registers
        bool any = false;
        for (int i = 1; i < 32; i++) {
            uint32_t cur = get_reg(top, i);
            if (cur != prev[i]) {
                if (any) printf("  ");
                printf("%s: 0x%08X → 0x%08X", RNAME[i], prev[i], cur);
                prev[i] = cur;
                any = true;
            }
        }
        if (!any) printf("(no change)");
        printf("\n");

        if (is_ecall) {
            printf("%s\n", std::string(100, '-').c_str());
            printf("HALT — ecall at PC=0x%04X   a0 = %u (0x%08X)\n",
                   pc, get_reg(top, 10), get_reg(top, 10));
            break;
        }
    }

    return 0;
}
