module clkdiv_tb;

logic rst, clk, divided;
logic [24:0] division;

clkdiv clkdiv(.*);

always #5 begin
  clk <= ~clk;
end

initial begin
  $monitor("%d: rst=%d clk=%d division=%d divided=%d",
           $time, rst, clk, division, divided);

  rst <= 1;
  clk <= 1;
  division <= 25'd1;

  #13
    rst <= 0;
    if (divided !== 1'b0) $error("divided must be 0");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b1) $error("divided must be 1");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b0) $error("divided must be 0");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b1) $error("divided must be 1");
    division <= 25'd2;

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b1) $error("divided must be 1");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b0) $error("divided must be 0");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b0) $error("divided must be 0");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b1) $error("divided must be 1");

  @(posedge clk)
  @(negedge clk)
    if (divided !== 1'b1) $error("divided must be 1");

  $finish;
end

endmodule
