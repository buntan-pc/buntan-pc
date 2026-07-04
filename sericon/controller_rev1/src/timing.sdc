# sys_clk: 27MHz Xtal
create_clock -period 37.0370 -name sys_clk -waveform {0 18.5185} [get_ports {sys_clk}]

# fclk: 204.429MHz rPLL output
# pclk: 204.429/5 = 40.8858MHz CLKDIV output
create_clock -period 4.89167 -name fclk -waveform {0 2.44584} [get_nets {fclk}]
create_clock -period 24.4584 -name pclk -waveform {0 14.6750} [get_nets {pclk}]

# usb_clk: 96MHz rPLL output
create_clock -period 10.4167 -name usb_clk -waveform {0 5.20833} [get_nets {usb_clk}]
# rPLL のジッタ（HCLK）の最大値は 30 mUI = 0.3125 ns
set_clock_uncertainty 0.5 -setup -from [get_clocks {usb_clk}] -to [get_clocks {usb_clk}]
set_clock_uncertainty 0.3125 -hold -from [get_clocks {usb_clk}] -to [get_clocks {usb_clk}]

set_clock_groups -asynchronous -group [get_clocks {sys_clk}] -group [get_clocks {fclk}]
set_clock_groups -asynchronous -group [get_clocks {sys_clk}] -group [get_clocks {pclk}]
set_clock_groups -asynchronous -group [get_clocks {sys_clk}] -group [get_clocks {usb_clk}]
