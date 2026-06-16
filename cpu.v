module cpu (
    input  wire        clk,
    input  wire        rst,

    // Testbench/debug initialization ports.
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
    output wire [31:0] dbg_x10,
    output wire [31:0] dbg_instr,
    output wire        dbg_rom_loading,

    output wire [23:0] video_rgb,
    output wire        video_hsync,
    output wire        video_vsync,
    output wire        video_de,
    output wire [9:0]  video_x,
    output wire [8:0]  video_y,
    output wire        video_frame_done,
    output wire [31:0] video_game_frame
);

localparam ALU_SUB = 4'b0001;
localparam WB_ALU  = 2'b00;
localparam WB_MEM  = 2'b01;
localparam WB_PC4  = 2'b10;
localparam GPU_FB_BASE  = 32'h0002_0000;
localparam GPU_FB_END   = 32'h0003_2C00;
localparam GPU_REG_BASE = 32'h0004_0000;
localparam GPU_REG_END  = 32'h0004_0100;
localparam IO_REG_BASE  = 32'h0005_0000;
localparam IO_REG_END   = 32'h0005_0100;

reg [31:0] pc;

wire [31:0] instr;
wire [6:0]  opcode = instr[6:0];
wire [4:0]  rd     = instr[11:7];
wire [2:0]  funct3 = instr[14:12];
wire [4:0]  rs1    = instr[19:15];
wire [4:0]  rs2    = instr[24:20];
wire [6:0]  funct7 = instr[31:25];

wire        alu_src;
wire [3:0]  alu_op;
wire        reg_write;
wire        mem_read;
wire        mem_write;
wire [1:0]  mem_to_reg;
wire        branch;
wire        branch_inv;
wire        jump;
wire        jalr;
wire        pc_to_alu;
wire [1:0]  mem_size;
wire        mem_unsigned;

wire [31:0] imm;
wire [31:0] rs1_data;
wire [31:0] rs2_data;
wire        lui = (opcode == 7'b0110111);
wire [31:0] alu_a = lui ? 32'b0 : (pc_to_alu ? pc : rs1_data);
wire [31:0] alu_b = alu_src ? imm : rs2_data;
wire [31:0] alu_y;
wire        alu_zero;
wire [31:0] ram_rd;
wire [31:0] gpu_rd;
wire [31:0] io_rd;
wire        gpu_addr_hit =
    ((alu_y >= GPU_FB_BASE)  && (alu_y < GPU_FB_END)) ||
    ((alu_y >= GPU_REG_BASE) && (alu_y < GPU_REG_END));
wire        io_addr_hit = (alu_y >= IO_REG_BASE) && (alu_y < IO_REG_END);
wire        normal_gpu_we = !rst & mem_write & gpu_addr_hit;
wire        normal_io_we  = !rst & mem_write & io_addr_hit;
wire        normal_ram_we = !rst & mem_write & !gpu_addr_hit & !io_addr_hit;
wire [31:0] mem_rd = gpu_addr_hit ? gpu_rd :
                     io_addr_hit  ? io_rd  :
                                    ram_rd;

wire [31:0] pc_plus_4 = pc + 32'd4;
wire [31:0] wb_data =
    (mem_to_reg == WB_MEM) ? mem_rd :
    (mem_to_reg == WB_PC4) ? pc_plus_4 :
                              alu_y;

// RF write: debug > normal (normal suppressed during rst)
wire        rf_we = dbg_rf_we ? 1'b1 : (!rst & reg_write);
wire [4:0]  rf_rd = dbg_rf_we ? dbg_rf_rd : rd;
wire [31:0] rf_wd = dbg_rf_we ? dbg_rf_wd : wb_data;

// ROM loader: streams rom_dump into RAM one word per cycle while rst=1
reg  [31:0] rom_idx;
wire [31:0] rom_data;
wire [31:0] rom_addr;
wire        rom_valid;
wire        rom_loading = rst & rom_valid;

initial rom_idx = 32'b0;

always @(posedge clk) begin
    if (!rst)         rom_idx <= 32'b0;
    else if (rom_valid) rom_idx <= rom_idx + 1;
end

rom_dump rom_loader (
    .word_idx (rom_idx),
    .data     (rom_data),
    .addr     (rom_addr),
    .valid    (rom_valid)
);

// RAM write: debug > rom_loading > normal (normal suppressed during rst)
wire        ram_we   = dbg_ram_we  ? 1'b1          :
                       rom_loading ? 1'b1          :
                                     normal_ram_we;
wire [31:0] ram_addr = dbg_ram_we  ? dbg_ram_addr  :
                       rom_loading ? rom_addr        :
                       (gpu_addr_hit | io_addr_hit) ? 32'b0 :
                                      alu_y;
wire [31:0] ram_wd   = dbg_ram_we  ? dbg_ram_wd    :
                       rom_loading ? rom_data       :
                                     rs2_data;
wire [1:0]  ram_size = (dbg_ram_we | rom_loading) ? 2'b10 : mem_size;
wire        ram_uns  = (dbg_ram_we | rom_loading) ? 1'b0  : mem_unsigned;

wire branch_predicate = (alu_op == ALU_SUB) ? alu_zero : alu_y[0];
wire branch_taken = branch & (branch_inv ^ branch_predicate);

wire [31:0] branch_target = pc + imm;
wire [31:0] jal_target = pc + imm;
wire [31:0] jalr_target = (rs1_data + imm) & 32'hFFFF_FFFE;
wire [31:0] next_pc =
    jalr         ? jalr_target :
    jump         ? jal_target :
    branch_taken ? branch_target :
                   pc_plus_4;

control ctrl (
    .opcode(opcode),
    .funct3(funct3),
    .funct7(funct7),
    .alu_src(alu_src),
    .alu_op(alu_op),
    .reg_write(reg_write),
    .mem_read(mem_read),
    .mem_write(mem_write),
    .mem_to_reg(mem_to_reg),
    .branch(branch),
    .branch_inv(branch_inv),
    .jump(jump),
    .jalr(jalr),
    .pc_to_alu(pc_to_alu),
    .mem_size(mem_size),
    .mem_unsigned(mem_unsigned)
);

imm_gen imm0 (
    .instr(instr),
    .imm(imm)
);

regfile rf (
    .clk(clk),
    .rs1(rs1),
    .rs2(rs2),
    .rd(rf_rd),
    .wd(rf_wd),
    .we(rf_we),
    .a(rs1_data),
    .b(rs2_data)
);

alu alu0 (
    .a(alu_a),
    .b(alu_b),
    .op(alu_op),
    .y(alu_y),
    .zero(alu_zero)
);

ram ram0 (
    .clk(clk),
    .iaddr(pc),
    .instr(instr),
    .daddr(ram_addr),
    .wd(ram_wd),
    .we(ram_we),
    .size(ram_size),
    .is_unsigned(ram_uns),
    .rd(ram_rd)
);

gpu gpu0 (
    .clk(clk),
    .rst(rst),
    .mmio_we(normal_gpu_we),
    .mmio_addr(alu_y),
    .mmio_wd(rs2_data),
    .mmio_size(mem_size),
    .mmio_rd(gpu_rd),
    .video_rgb(video_rgb),
    .video_hsync(video_hsync),
    .video_vsync(video_vsync),
    .video_de(video_de),
    .video_x(video_x),
    .video_y(video_y),
    .video_frame_done(video_frame_done),
    .dbg_user_counter(video_game_frame)
);

io io0 (
    .clk(clk),
    .rst(rst),
    .mmio_we(normal_io_we),
    .mmio_addr(alu_y),
    .mmio_wd(rs2_data),
    .mmio_size(mem_size),
    .mmio_rd(io_rd)
);

always @(posedge clk) begin
    if (rst)
        pc <= 32'b0;
    else
        pc <= next_pc;
end

assign dbg_pc = pc;
assign dbg_x1 = rf.regs[1];
assign dbg_x2 = rf.regs[2];
assign dbg_x3 = rf.regs[3];
assign dbg_x4 = rf.regs[4];
assign dbg_x10 = rf.regs[10];
assign dbg_instr = instr;
assign dbg_rom_loading = rom_loading;

endmodule
