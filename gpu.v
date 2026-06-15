module gpu #(
    parameter FB_BASE  = 32'h0002_0000,
    parameter REG_BASE = 32'h0004_0000
) (
    input  wire        clk,
    input  wire        rst,

    input  wire        mmio_we,
    input  wire [31:0] mmio_addr,
    input  wire [31:0] mmio_wd,
    input  wire [1:0]  mmio_size,
    output reg  [31:0] mmio_rd,

    output reg  [23:0] video_rgb,
    output reg         video_hsync,
    output reg         video_vsync,
    output reg         video_de,
    output reg  [9:0]  video_x,
    output reg  [8:0]  video_y,
    output reg         video_frame_done,
    output wire [31:0] dbg_user_counter
);

localparam FB_W = 160;
localparam FB_H = 120;
localparam OUT_W = 800;
localparam OUT_H = 480;
localparam FB_PIXELS = FB_W * FB_H;
localparam FB_BYTES = FB_PIXELS * 4;

reg [23:0] fb [0:FB_PIXELS-1];
reg [31:0] user_counter;

reg [9:0] out_x;
reg [8:0] out_y;
reg [7:0] src_x;
reg [6:0] src_y;
reg [2:0] scale_x;
reg [1:0] scale_y;
reg [31:0] frame_counter;

wire fb_addr_hit = (mmio_addr >= FB_BASE) && (mmio_addr < (FB_BASE + FB_BYTES));
wire reg_addr_hit = (mmio_addr >= REG_BASE) && (mmio_addr < (REG_BASE + 32'h0000_0100));
wire [31:0] fb_index = (mmio_addr - FB_BASE) >> 2;
wire [31:0] scan_index = ({25'b0, src_y} * 32'd160) + {24'b0, src_x};

always @(*) begin
    if (fb_addr_hit)
        mmio_rd = {8'b0, fb[fb_index]};
    else if (reg_addr_hit) begin
        case (mmio_addr - REG_BASE)
            32'h00: mmio_rd = FB_W;
            32'h04: mmio_rd = FB_H;
            32'h08: mmio_rd = OUT_W;
            32'h0C: mmio_rd = OUT_H;
            32'h10: mmio_rd = frame_counter;
            32'h14: mmio_rd = user_counter;
            default: mmio_rd = 32'b0;
        endcase
    end else begin
        mmio_rd = 32'b0;
    end
end

always @(posedge clk) begin
    if (rst) begin
        out_x <= 10'b0;
        out_y <= 9'b0;
        src_x <= 8'b0;
        src_y <= 7'b0;
        scale_x <= 3'b0;
        scale_y <= 2'b0;
        video_rgb <= 24'b0;
        video_hsync <= 1'b0;
        video_vsync <= 1'b0;
        video_de <= 1'b0;
        video_x <= 10'b0;
        video_y <= 9'b0;
        video_frame_done <= 1'b0;
        frame_counter <= 32'b0;
        user_counter <= 32'b0;
    end else begin
        if (mmio_we && fb_addr_hit && (mmio_size == 2'b10))
            fb[fb_index] <= mmio_wd[23:0];
        if (mmio_we && reg_addr_hit && (mmio_size == 2'b10) &&
            ((mmio_addr - REG_BASE) == 32'h14))
            user_counter <= mmio_wd;

        video_rgb <= fb[scan_index];
        video_hsync <= (out_x == 10'd0);
        video_vsync <= (out_x == 10'd0) && (out_y == 9'd0);
        video_de <= 1'b1;
        video_x <= out_x;
        video_y <= out_y;
        video_frame_done <= 1'b0;

        if (out_x == OUT_W-1) begin
            out_x <= 10'b0;
            src_x <= 8'b0;
            scale_x <= 3'b0;

            if (out_y == OUT_H-1) begin
                out_y <= 9'b0;
                src_y <= 7'b0;
                scale_y <= 2'b0;
                video_frame_done <= 1'b1;
                frame_counter <= frame_counter + 1;
            end else begin
                out_y <= out_y + 1;
                if (scale_y == 2'd3) begin
                    scale_y <= 2'b0;
                    src_y <= src_y + 1;
                end else begin
                    scale_y <= scale_y + 1;
                end
            end
        end else begin
            out_x <= out_x + 1;
            if (scale_x == 3'd4) begin
                scale_x <= 3'b0;
                src_x <= src_x + 1;
            end else begin
                scale_x <= scale_x + 1;
            end
        end
    end
end

integer _i;
initial begin
    for (_i = 0; _i < FB_PIXELS; _i = _i + 1)
        fb[_i] = 24'b0;
end

assign dbg_user_counter = user_counter;

endmodule
