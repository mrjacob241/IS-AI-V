module ram #(
    parameter BYTES = 131072   // 128 KB
) (
    input  wire        clk,

    // instruction fetch — combinational word read
    input  wire [31:0] iaddr,
    output wire [31:0] instr,

    // data port — combinational read, synchronous byte-enable write
    input  wire [31:0] daddr,
    input  wire [31:0] wd,
    input  wire        we,
    input  wire [1:0]  size,         // 00=byte  01=half  10=word
    input  wire        is_unsigned,
    output reg  [31:0] rd
);

reg [7:0] mem [0:BYTES-1];

// instruction fetch: little-endian word assembly
assign instr = {mem[iaddr+3], mem[iaddr+2], mem[iaddr+1], mem[iaddr]};

// data read: intermediate values, updated combinationally
wire  [7:0] byte_val = mem[daddr];
wire [15:0] half_val = {mem[daddr+1], mem[daddr]};
wire [31:0] word_val = {mem[daddr+3], mem[daddr+2], mem[daddr+1], mem[daddr]};

always @(*) begin
    case (size)
        2'b00: rd = is_unsigned ? {24'b0,          byte_val}
                                : {{24{byte_val[7]}}, byte_val};
        2'b01: rd = is_unsigned ? {16'b0,          half_val}
                                : {{16{half_val[15]}}, half_val};
        default: rd = word_val;
    endcase
end

// data write: synchronous, byte-enable masked by size
always @(posedge clk) begin
    if (we) begin
        case (size)
            2'b00: mem[daddr] <= wd[7:0];
            2'b01: begin
                mem[daddr]   <= wd[7:0];
                mem[daddr+1] <= wd[15:8];
            end
            default: begin
                mem[daddr]   <= wd[7:0];
                mem[daddr+1] <= wd[15:8];
                mem[daddr+2] <= wd[23:16];
                mem[daddr+3] <= wd[31:24];
            end
        endcase
    end
end

integer _i;
initial begin
    for (_i = 0; _i < BYTES; _i = _i + 1)
        mem[_i] = 8'b0;
end

endmodule
