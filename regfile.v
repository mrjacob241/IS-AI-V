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

reg [31:0] regs [0:31];

assign a = (rs1 == 5'b0) ? 32'b0 : regs[rs1];
assign b = (rs2 == 5'b0) ? 32'b0 : regs[rs2];

always @(posedge clk) begin
    if (we && rd != 5'b0)
        regs[rd] <= wd;
end

integer i;
initial begin
    for (i = 0; i < 32; i = i + 1)
        regs[i] = 32'b0;
end

endmodule
