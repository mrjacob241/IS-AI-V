module io #(
    parameter REG_BASE = 32'h0005_0000
) (
    input  wire        clk,
    input  wire        rst,

    input  wire        mmio_we,
    input  wire [31:0] mmio_addr,
    input  wire [31:0] mmio_wd,
    input  wire [1:0]  mmio_size,
    output reg  [31:0] mmio_rd
);

reg [31:0] key_state;
reg [31:0] stream_char;
reg [31:0] stream_seq;
reg [31:0] stream_ack;

wire reg_addr_hit = (mmio_addr >= REG_BASE) && (mmio_addr < (REG_BASE + 32'h0000_0100));
wire [31:0] reg_offset = mmio_addr - REG_BASE;

always @(*) begin
    if (reg_addr_hit) begin
        case (reg_offset)
            32'h00: mmio_rd = key_state;
            32'h04: mmio_rd = stream_char;
            32'h08: mmio_rd = stream_seq;
            32'h0C: mmio_rd = stream_ack;
            default: mmio_rd = 32'b0;
        endcase
    end else begin
        mmio_rd = 32'b0;
    end
end

always @(posedge clk) begin
    if (rst) begin
        key_state <= 32'b0;
        stream_char <= 32'b0;
        stream_seq <= 32'b0;
        stream_ack <= 32'b0;
    end else if (mmio_we && reg_addr_hit && (mmio_size == 2'b10) &&
                 (reg_offset == 32'h00)) begin
        key_state <= mmio_wd;
    end else if (mmio_we && reg_addr_hit && (mmio_size == 2'b10) &&
                 (reg_offset == 32'h04)) begin
        stream_char <= mmio_wd;
    end else if (mmio_we && reg_addr_hit && (mmio_size == 2'b10) &&
                 (reg_offset == 32'h08)) begin
        stream_seq <= mmio_wd;
    end else if (mmio_we && reg_addr_hit && (mmio_size == 2'b10) &&
                 (reg_offset == 32'h0C)) begin
        stream_ack <= mmio_wd;
    end
end

endmodule
