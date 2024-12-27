module cdtimer_tb;

logic rst, clk, load, timeout;
logic [3:0] data, counter;

cdtimer#(.PERIOD(3), .WIDTH(4)) cdtimer(.*);

// 10 単位時間周期のクロックを生成
always #5 begin
  clk <= ~clk;
end

integer i, j;

initial begin
  $monitor("%d: rst=%d load=%d data=%d counter=%d timeout=%d period=%d",
           $time, rst, load, data, counter, timeout, cdtimer.period);

  rst <= 1;
  clk <= 1;
  load <= 1;
  data <= 4'd2;

  #13
    rst <= 0;
    test_timer(4'd0, 0, 16'd0);

  @(posedge clk)
    load <= 0;
  @(negedge clk)
    test_timer(4'd2, 0, 16'd0);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd2, 0, 16'd1);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd2, 0, 16'd2);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd1, 0, 16'd0);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd1, 0, 16'd1);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd1, 0, 16'd2);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd0, 1, 16'd0);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd0, 1, 16'd0);

  @(posedge clk)
    load <= 1;
    data <= 4'd5;
  @(negedge clk)
    test_timer(4'd0, 1, 16'd0);

  @(posedge clk)
    load <= 0;
  @(negedge clk)
    test_timer(4'd5, 0, 16'd0);

  @(posedge clk)
    load <= 1;
    data <= 4'd10;
  @(negedge clk)
    test_timer(4'd5, 0, 16'd1);

  @(posedge clk)
    load <= 0;
  @(negedge clk)
    test_timer(4'd10, 0, 16'd0);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd10, 0, 16'd1);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd10, 0, 16'd2);

  for (i = 9; i > 0; i = i - 1) begin
    for (j = 0; j < 3; j = j + 1) begin
      @(posedge clk)
      @(negedge clk)
        test_timer(i, 0, j);
    end
  end

  // タイムアウトフラグをクリアできることを確認
  @(posedge clk)
    load <= 1;
    data <= 4'd0;
  @(negedge clk)
    test_timer(4'd0, 1, 16'd0);

  @(posedge clk)
    load <= 0;
  @(negedge clk)
    test_timer(4'd0, 0, 16'd0);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd0, 0, 16'd1);

  @(posedge clk)
  @(negedge clk)
    test_timer(4'd0, 0, 16'd2);

  // period が 1 週後もタイムアウトフラグが 0 であることを確認
  @(posedge clk)
  @(negedge clk)
    test_timer(4'd0, 0, 16'd0);

  $finish;
end

task test_timer(input [3:0] e_counter, input e_timeout, input [15:0] e_period);
begin
  if (e_counter !== 4'hx && counter !== e_counter)
    $error("counter must be %d", e_counter);
  if (e_timeout !== 1'bx && timeout !== e_timeout)
    $error("timeout must be %d", e_timeout);
  if (e_period !== 16'hxxxx && cdtimer.period !== e_period)
    $error("period must be %d", e_period);
end
endtask

endmodule
