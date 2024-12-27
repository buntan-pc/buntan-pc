module simple_timer#(
  parameter PERIOD=27_000_000/9600,
  parameter BITS=24
) (
  input rst, clk,
  output half, full
);

logic [BITS-1:0] cnt, inc, next;

assign inc  = cnt + 1;
assign next = inc < PERIOD ? inc : 0;
assign full = next == {BITS{1'b0}};
assign half = cnt == (PERIOD >> 1);

always @(posedge rst, posedge clk) begin
  if (rst)
    cnt <= 0;
  else
    cnt <= next;
end

endmodule

