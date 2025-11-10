module memory_control (
    addr_base,
    clock,
    operation,
    current_zoom,
    enable,
    addr_out_rd,
    addr_out_wr,
    done,
    wr_enable,
    counter_op,
    color_in,
    color_out,
    finish_state,

    current_state


);
output reg finish_state;

    output [2:0] current_state;
    input [7:0] color_in;
    output reg [7:0] color_out;

    assign current_state = state;
    // estado de aguardar uma nova instrução
    localparam IDLE = 3'b000;

    //operações de leitura ou escrita na memoria
    localparam RD_DATA = 3'b001;
    localparam WR_DATA = 3'b010;

    //algoritmos
    localparam NHI_ALG = 3'b011;
    localparam PR_ALG = 3'b100;
    localparam NH_ALG = 3'b101;
    localparam BA_ALG = 3'b110;
    
    //estado apenas para aguardar finalizar a operação de leitura ou escrita
    localparam WAIT_WR_RD = 3'b111;

    input [16:0] addr_base;
    input [2:0] operation;
    input [2:0] current_zoom;
    input enable, clock;
    output reg [16:0] addr_out_rd;
    output reg [16:0] addr_out_wr;
    output reg done;
    output reg wr_enable;
    output [2:0] counter_op;

    reg [2:0] state;

    reg [1:0] wr_wait_counter;
    reg [16:0] algorithm_needed_steps;
    reg [16:0] algorithm_current_step;



    reg [2:0] current_operation_step;
    reg has_alg_on_exec;

    reg [10:0] old_x, old_y, new_x, new_y;

    reg [2:0] offset;
    
    assign counter_op = current_operation_step;

    reg [16:0] addr_base_wr;
    reg [16:0] addr_base_rd;
    
    always @(posedge clock) begin
        case (state)
            IDLE:   begin
                done <= 1'b1;
                has_alg_on_exec <= 1'b0;
                wr_enable <= 1'b0;
                addr_out_rd <= 17'b0;
                addr_out_wr <= 17'b0;
                if (enable) begin
                    state <= operation;
                    done <= 1'b0;
                end else begin
                    state <= IDLE;
                end
            end

            WAIT_WR_RD: begin // adicionar a veri
                if ( wr_wait_counter == 2'b01) begin
                    if(operation == PR_ALG) begin
                        if (current_operation_step == 3'b000) begin //guarda a cor a ser escrita
                            color_out <= color_in;
                        end else begin
                            color_out <= color_out; //mantem salva a cor a ser escrita nas proximas operações
                        end
                    end else begin
                        color_out <= color_in;
                    end
                    if (operation == RD_DATA || operation == WR_DATA) begin
                        state <= IDLE;
                        wr_wait_counter <= 2'b00;
                        wr_enable <= 1'b0;
                        done <= 1'b1;
                    end else begin
                        if (algorithm_current_step >= algorithm_needed_steps) begin
                            state <= IDLE;
                            wr_wait_counter <= 2'b00;
                            wr_enable <= 1'b0;
                            done <= 1'b1;
                        end else begin 
                            wr_enable <= 1'b0;
                            state <= operation;
                        end
                    end
                end else begin
                    wr_wait_counter <= wr_wait_counter + 1;
                    wr_enable <= wr_enable;
                    state <= WAIT_WR_RD;
                end
            end

            RD_DATA: begin
                addr_out_rd <= addr_base;
                state <= WAIT_WR_RD;
                wr_wait_counter <= 2'b00;
                wr_enable <= 1'b0;
                done <= 1'b0;
            end

            WR_DATA: begin
                addr_out_wr <= addr_base;
                state <= WAIT_WR_RD;
                wr_wait_counter <= 2'b00;
                wr_enable <= 1'b1;
                done <= 1'b0;
            end

            PR_ALG: begin
                if (!has_alg_on_exec) begin
                    has_alg_on_exec <= 1'b1;
                    algorithm_needed_steps <= 19'd19200;
                    algorithm_current_step <= 17'd0;
                    current_operation_step <= 3'b0;
                    old_x <= 10'd0;
                    old_y <= 10'd0;
                    new_x <= 10'd0;
                    new_y <= 10'd0;
                    state <= PR_ALG;
                end else begin
                    case (current_operation_step)

                        3'b000: begin //leitura
                            addr_out_rd <= old_x + (old_y*10'd320);
                            wr_wait_counter <= 2'b00;
                            wr_enable <= 1'b0;
                            state <= WAIT_WR_RD;
                            current_operation_step <= 3'b001;
                            done <= 1'b0;
                        end
                        3'b001: begin //escrita 00
                            addr_out_wr <= new_x + (new_y*320);
                            current_operation_step <= 3'b010;
                            wr_enable <= 1'b1;
                            wr_wait_counter <= 2'b00;
                            done <= 1'b0;
                            new_x <= new_x + 1'b1;
                            state <= WAIT_WR_RD;
                        end
                        3'b010: begin //escrita 01
                            addr_out_wr <= new_x + (new_y*320);
                            current_operation_step <= 3'b011;
                            wr_enable <= 1'b1;
                            wr_wait_counter <= 2'b00;
                            done <= 1'b0;
                            new_x <= new_x - 1'b1;
                            new_y <= new_y + 1'b1;
                            state <= WAIT_WR_RD;
                        end
                        3'b100: begin //escrita 10
                            addr_out_wr <= new_x + (new_y*320);
                            current_operation_step <= 3'b101;
                            wr_enable <= 1'b1;
                            wr_wait_counter <= 2'b00;
                            done <= 1'b0;
                            new_x <= new_x + 1'b1;
                            state <= WAIT_WR_RD;
                        end
                        3'b101: begin //escrita 11
                            addr_out_wr <= new_x + (new_y*320);
                            current_operation_step <= 3'b000;
                            wr_enable <= 1'b1;
                            wr_wait_counter <= 2'b00;
                            done <= 1'b0;
                            if (new_x == 10'd319) begin
                                new_x <= 10'd0;
                                new_y <= new_y + 1'b1;
                                old_x <= 10'd0;
                                old_y <= old_y + 1'b1;
                            end else begin
                                new_x <= new_x + 1'b1;
                                new_y <= new_y - 1'b1;
                                old_x <= old_x + 1'b1;
                            end
                        end
                    endcase
                end
            end

            // NHI_ALG: begin
            //     if (!has_alg_on_exec) begin
            //         has_alg_on_exec <= 1'b1;
            //         algorithm_needed_steps <= 17'd19200;
            //         algorithm_current_step <= 17'd0;
            //         current_operation_step <= 3'b0;
            //         addr_base_rd <= 17'd19200;
            //         addr_base_wr <= 17'd0;
            //         state <= NHI_ALG;
            //         old_x <= 10'd0;
            //         old_y <= 10'd0;
            //         new_x <= 10'd0;
            //         new_y <= 10'd0;
            //     end else begin
            //         case (current_operation_step)
            //             3'b000: begin
            //                 addr_out_rd <= ( old_x<<1) + ((old_y<<1)*10'd320);
            //                 wr_wait_counter <= 2'b00;
            //                 wr_enable <= 1'b0;
            //                 state <= WAIT_WR_RD;
            //                 current_operation_step <= 3'b010;
            //                 // addr_base_rd <= addr_base_rd + 2'd2;
            //                 // current_operation_step <= 3'b001;
            //                 // wr_enable <= 1'b0;
            //                 // state <= NHI_ALG;
            //                 // done <= 1'b0;
            //             end
            //             3'b001: begin
            //                 //addr_out_rd <= addr_out;
            //                 wr_wait_counter <= 2'b00;
            //                 wr_enable <= 1'b0;
            //                 state <= WAIT_WR_RD;
            //                 current_operation_step <= 3'b010;
            //             end
            //             3'b010: begin
            //                 finish_state <= 1'b0;
                            
            //                 addr_out_wr <= new_x + (new_y*10'd320);

            //                 algorithm_current_step <= algorithm_current_step + 1;
            //                 //addr_out_wr <= addr_out;
            //                 wr_enable <= 1'b1;
            //                 wr_wait_counter <= 2'b00;
            //                 // if (new_x == 10'd319) begin
            //                 //     new_x <= 10'd0;
            //                 //     new_y <= new_y + 1;
            //                 //     old_y <= (new_y >> 1'b1) + 10'd60;
            //                 //     old_x <= 10'd80;
            //                 // end else begin
            //                 //     new_x <= new_x + 1;
            //                 //     old_x <= (new_x >> 1'b1) + 10'd80;
            //                 // end
            //                 // state <= WAIT_WR_RD;
            //                 // state <= NHI_ALG;
            //                 // current_operation_step <= 3'b000;
            //                 // addr_base_wr <= addr_base_wr+1'b1;
            //                 current_operation_step <= 3'b011;
            //                 state <= WAIT_WR_RD;
            //                 done <= 1'b0;
                            
            //             end

            //             3'b011: begin
            //                 //algorithm_current_step <= algorithm_current_step + 1;
            //                 //addr_out_wr <= addr_out;
            //                 wr_enable <= 1'b0;
            //                 wr_wait_counter <= 2'b00;
            //                 if (new_x == 10'd319) begin
            //                     new_x <= 10'd0;
            //                     new_y <= new_y + 1;
            //                     old_y <= (new_y >> 1'b1);
            //                     old_x <= 10'd0;
            //                 end else begin
            //                     new_x <= new_x + 1;
            //                     old_x <= (new_x >> 1'b1);
            //                 end
            //                 state <= NHI_ALG;
            //                 //state <= NHI_ALG;
            //                 current_operation_step <= 3'b000;
            //             end
            //             default: begin
            //                 finish_state <= 1'b0;
            //                 current_operation_step <= 3'b0;
            //                 state <= NHI_ALG;
            //             end
            //         endcase
            //     end
            // end
            // NH_ALG: begin
            //     if (!has_alg_on_exec) begin
            //         algorithm_needed_steps <= (current_zoom == 3'b100) ? 19'd19200:19'd4800;
            //         algorithm_current_step <= 17'd0;
                    
            //         old_x <= 10'd0;
            //         old_y <= 10'd0;
            //         new_x <= (current_zoom == 3'b100) ? 10'd80:10'd120;
            //         new_y <= (current_zoom == 3'b100) ? 10'd60:10'd90;
            //         has_alg_on_exec <= 1'b1;
            //         offset <= (current_zoom == 3'b100) ? 3'd2:3'd4;
            //         current_operation_step <= 3'b0;
            //         state <= NH_ALG;
            //     end else begin

            //         case (current_operation_step)
            //             3'b000: begin
            //                 addr_out <= ((old_x) + ((old_y)*10'd320));
                            
                            
            //                 wr_enable <= 1'b0;
                            
            //                 done <= 1'b0;
            //                 if (old_x >= 10'd316) begin
            //                     old_x <= 10'd0;
            //                     old_y <= old_y + offset;
            //                 end else begin
            //                     old_x <= old_x + offset;
            //                 end
            //                 wr_wait_counter <= 2'b00;
            //                 current_operation_step <= 3'b001;
            //                 state <= WAIT_WR_RD;
            //             end
            //             3'b001: begin
            //                 addr_out <= addr_out;
            //                 wr_wait_counter <= 2'b00;
            //                 wr_enable <= 1'b0;
            //                 done <= 1'b0;
            //                 current_operation_step <= 3'b010;
            //                 state <= NH_ALG;
                            
            //             end
            //             3'b010: begin
            //                 addr_out <= new_x + (new_y*320);
            //                 current_operation_step <= 3'b100;
                            
            //                 wr_enable <= 1'b1;
            //                 wr_wait_counter <= 2'b00;
            //                 done <= 1'b0;
            //                 algorithm_current_step <= algorithm_current_step + 1;
            //                 if ((new_x == 10'd200 && offset == 3'd4) || (new_x == 10'd240 && offset == 3'd2)) begin
            //                     new_x <= 10'd0;
            //                     new_y <= new_y + 1;
            //                 end else begin
            //                     new_x <= new_x + 1;
            //                 end
            //                 state <= WAIT_WR_RD;
            //             end
            //             3'b100: begin
            //                 state <= NH_ALG;
            //                 current_operation_step <= 3'b0;
            //             end
                        
            //         endcase
            //     end
            // end
            // PR_ALG: begin
            //     if (!has_alg_on_exec) begin
            //         has_alg_on_exec <= 1'b1;
            //         algorithm_needed_steps <= 19'd19200;
            //         algorithm_current_step <= 17'd0;
            //         current_operation_step <= 3'b0;
            //         old_x <= 10'd80;
            //         old_y <= 10'd60;
            //         new_x <= 10'd0;
            //         new_y <= 10'd0;
            //         state <= PR_ALG;
            //     end else begin
            //         case (current_operation_step)
            //             3'b000: begin
            //                 addr_out <= old_x + (old_y*10'd320);
            //                 current_operation_step <= 3'b001;
            //                 wr_wait_counter <= 2'b00;
            //                 wr_enable <= 1'b0;
            //                 state <= WAIT_WR_RD;
                            
            //                 done <= 1'b0;
            //             end
            //         endcase

            //     end
            // end

        endcase
    end


endmodule