module zoom_out_one (
    enable,
    data_in,
    data_out
);
    input enable;
    input [31:0] data_in;
    output [7:0] data_out;

    wire [10:0] pixel_sum;

    assign pixel_sum = data_in[7:0] + data_in[15:8] + data_in[23:16] + data_in[31:24];
    assign data_out = (enable) ? pixel_sum >> 2 : 8'b00000000;

endmodule