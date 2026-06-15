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
    output reg         branch_inv,  // 1 = invert branch condition (BNE/BGE/BGEU)
    output reg         jump,
    output reg         jalr,
    output reg         pc_to_alu,
    output reg  [1:0]  mem_size,
    output reg         mem_unsigned
);

localparam ALU_ADD  = 4'b0000;
localparam ALU_SUB  = 4'b0001;
localparam ALU_AND  = 4'b0010;
localparam ALU_OR   = 4'b0011;
localparam ALU_XOR  = 4'b0100;
localparam ALU_SLL  = 4'b0101;
localparam ALU_SRL  = 4'b0110;
localparam ALU_SRA  = 4'b0111;
localparam ALU_SLT  = 4'b1000;
localparam ALU_SLTU = 4'b1001;

localparam WB_ALU = 2'b00;
localparam WB_MEM = 2'b01;
localparam WB_PC4 = 2'b10;

// Derive ALU op from funct3/funct7 for R-type and I-type ALU instructions.
// is_r=1 enables SUB (R-type only; ADDI with funct7!=0 is illegal).
function [3:0] alu_decode;
    input [2:0] f3;
    input       f7_5;
    input       is_r;
    begin
        case (f3)
            3'b000: alu_decode = (f7_5 & is_r) ? ALU_SUB : ALU_ADD;
            3'b001: alu_decode = ALU_SLL;
            3'b010: alu_decode = ALU_SLT;
            3'b011: alu_decode = ALU_SLTU;
            3'b100: alu_decode = ALU_XOR;
            3'b101: alu_decode = f7_5 ? ALU_SRA : ALU_SRL;
            3'b110: alu_decode = ALU_OR;
            3'b111: alu_decode = ALU_AND;
            default: alu_decode = ALU_ADD;
        endcase
    end
endfunction

always @(*) begin
    // safe defaults — every output driven every cycle
    alu_src      = 1'b0;
    alu_op       = ALU_ADD;
    reg_write    = 1'b0;
    mem_read     = 1'b0;
    mem_write    = 1'b0;
    mem_to_reg   = WB_ALU;
    branch       = 1'b0;
    branch_inv   = 1'b0;
    jump         = 1'b0;
    jalr         = 1'b0;
    pc_to_alu    = 1'b0;
    mem_size     = 2'b10;
    mem_unsigned = 1'b0;

    case (opcode)

        7'b0110011: begin   // R-type
            reg_write = 1;
            alu_op    = alu_decode(funct3, funct7[5], 1'b1);
        end

        7'b0010011: begin   // I-type ALU
            alu_src   = 1;
            reg_write = 1;
            alu_op    = alu_decode(funct3, funct7[5], 1'b0);
        end

        7'b0000011: begin   // I-type load
            alu_src    = 1;
            reg_write  = 1;
            mem_read   = 1;
            mem_to_reg = WB_MEM;
            alu_op     = ALU_ADD;
            case (funct3)
                3'b000: begin mem_size = 2'b00; mem_unsigned = 0; end // LB
                3'b001: begin mem_size = 2'b01; mem_unsigned = 0; end // LH
                3'b010: begin mem_size = 2'b10; mem_unsigned = 0; end // LW
                3'b100: begin mem_size = 2'b00; mem_unsigned = 1; end // LBU
                3'b101: begin mem_size = 2'b01; mem_unsigned = 1; end // LHU
                default: begin mem_size = 2'b10; mem_unsigned = 0; end
            endcase
        end

        7'b0100011: begin   // S-type store
            alu_src   = 1;
            mem_write = 1;
            alu_op    = ALU_ADD;
            case (funct3)
                3'b000: mem_size = 2'b00; // SB
                3'b001: mem_size = 2'b01; // SH
                3'b010: mem_size = 2'b10; // SW
                default: mem_size = 2'b10;
            endcase
        end

        7'b1100011: begin   // B-type branch
            branch     = 1;
            branch_inv = funct3[0];     // BNE/BGE/BGEU invert condition
            case (funct3)
                3'b000, 3'b001: alu_op = ALU_SUB;  // BEQ, BNE  → check zero
                3'b100, 3'b101: alu_op = ALU_SLT;  // BLT, BGE  → check bit 0
                3'b110, 3'b111: alu_op = ALU_SLTU; // BLTU, BGEU→ check bit 0
                default:        alu_op = ALU_SUB;
            endcase
        end

        7'b1101111: begin   // JAL
            reg_write  = 1;
            mem_to_reg = WB_PC4;
            jump       = 1;
        end

        7'b1100111: begin   // JALR
            alu_src    = 1;
            reg_write  = 1;
            mem_to_reg = WB_PC4;
            jump       = 1;
            jalr       = 1;
            alu_op     = ALU_ADD;
        end

        7'b0110111: begin   // LUI  (rs1 field encodes x0, so 0+imm = imm)
            alu_src   = 1;
            reg_write = 1;
            alu_op    = ALU_ADD;
        end

        7'b0010111: begin   // AUIPC
            alu_src   = 1;
            reg_write = 1;
            pc_to_alu = 1;
            alu_op    = ALU_ADD;
        end

        7'b1110011: begin   // SYSTEM (ecall/ebreak — treated as halt)
            // all outputs remain at safe defaults
        end

        default: begin
            // unknown opcode — safe defaults already set above
        end

    endcase
end

endmodule
