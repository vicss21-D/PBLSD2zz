module level_to_pulse (
    level_in, clk_in, pulse_out
);
    input level_in, clk_in;
    output pulse_out;
    reg delay_key;
	 

     always @(posedge clk_in) begin
        if (level_in) begin
            delay_key <= 1'b1;
        end else begin
            delay_key <= 1'b0;
        end
     end  
	

    assign pulse_out = !delay_key & level_in;

endmodule