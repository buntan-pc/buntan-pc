module signalizer_tb;

logic rst, clk, phase_decode, phase_exec, phase_rdmem, phase_fetch;
logic [1:0] clk_count;
logic [3:0] phases;

signalizer signalizer(.*);

assign phases = {phase_decode, phase_exec, phase_rdmem, phase_fetch};

always #5 begin
  clk <= ~clk;
end

always @(posedge clk) begin
  clk_count <= 2'd1 + clk_count;
end

initial begin
  $monitor("%d: rst=%d clk_count=%d phase=(%d %d %d %d)",
           $time, rst, clk_count,
           phase_decode, phase_exec, phase_rdmem, phase_fetch);

  rst <= 1;
  clk <= 1;
  clk_count <= 0;

  #13
    rst <= 0;
    if (phases !== 4'b0010) $error("signalizer must start from 0b0010");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b0001) $error("phases must be 0b0001");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b1000) $error("phases must be 0b1000");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b0100) $error("phases must be 0b0100");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b0010) $error("phases must be 0b0010");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b0001) $error("phases must be 0b0001");

  @(posedge clk)
  @(negedge clk)
    if (phases !== 4'b1000) $error("phases must be 0b1000");

  $finish;
end

endmodule
