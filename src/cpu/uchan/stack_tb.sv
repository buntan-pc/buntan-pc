module stack_tb;

localparam DEPTH = 3;

logic rst, clk, pop, push, load;
logic [15:0] data_in, data0, data1;

stack#(.DEPTH(DEPTH)) stk(.*);

// 10 単位時間周期のクロックを生成
always #5 begin
  clk <= ~clk;
end

initial begin
  $monitor("%d: rst=%d pop=%d push=%d in=%04x data0=%04x data1=%04x",
           $time, rst, pop, push, data_in, data0, data1);

  rst <= 1;
  clk <= 1;
  pop <= 0;
  push <= 1;
  load <= 1;
  data_in <= 16'hCAFE;

  #13
    rst <= 0;
    test_data(16'h0000, 16'h0000);

  @(posedge clk)
    push <= 0;
    load <= 0;
    data_in <= 16'hDEAD;
  @(negedge clk)
    test_data(16'hCAFE, 16'h0000);

  @(posedge clk)
    push <= 1;
    load <= 1;
  @(negedge clk)
    test_data(16'hCAFE, 16'h0000);

  @(posedge clk)
    data_in <= 16'hBEEF;
  @(negedge clk)
    test_data(16'hDEAD, 16'hCAFE);

  @(posedge clk)
    push <= 0;
    pop <= 1;
    load <= 1;
    data_in <= 16'hFACE;
  @(negedge clk)
    test_data(16'hBEEF, 16'hDEAD);

  @(posedge clk)
    load <= 0;

  @(negedge clk)
    test_data(16'hFACE, 16'hCAFE);

  @(posedge clk)
    pop <= 0;
  @(negedge clk)
    test_data(16'hCAFE, 16'h0000);

  @(posedge clk)
    push <= 1;
    load <= 1;
    data_in <= 16'h0001;
  @(negedge clk)
    test_data(16'hCAFE, 16'h0000);

  @(posedge clk)
    data_in <= 16'h0002;
  @(negedge clk)
    test_data(16'h0001, 16'hCAFE);

  @(posedge clk)
    data_in <= 16'h0003;
  @(negedge clk)
    test_data(16'h0002, 16'h0001);

  @(posedge clk)
    push <= 0;
    pop <= 1;
    load <= 0;
  @(negedge clk)
    test_data(16'h0003, 16'h0002);

  @(negedge clk)
    test_data(16'h0002, 16'h0001);

  @(negedge clk)
    test_data(16'h0001, 16'h0000);

  @(negedge clk)
    test_data(16'h0000, 16'h0000);

  @(negedge clk)
    test_data(16'h0000, 16'h0000);

  @(posedge clk)
    push <= 1;
    pop <= 0;
    load <= 1;
    data_in <= 16'hFACE;

  @(posedge clk)
    push <= 0;
    data_in <= 16'hAAAA;
  @(negedge clk)
    test_data(16'hFACE, 16'h0000);

  @(posedge clk)
    push <= 0;
    pop <= 1;
    load <= 0;
  @(negedge clk)
    test_data(16'hAAAA, 16'h0000);

  @(negedge clk)
    test_data(16'h0000, 16'h0000);

  $finish;
end

task test_data(
  input [15:0] e_data0,
  input [15:0] e_data1);
begin
  if (data0 !== 16'hxxxx && data0 !== e_data0)
    $error("data0 must be 0x%04x", e_data0);
  if (data1 !== 16'hxxxx && data1 !== e_data1)
    $error("data1 must be 0x%04x", e_data1);
end
endtask

endmodule
