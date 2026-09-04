create_clock -period 37.0370 -name sys_clk -waveform {0 18.5185} [get_ports {sys_clk}]
#create_clock -period 49.3827 -name clk_cpu -waveform {0 24.6914} [get_nets {clk_cpu}]
#create_clock -period 98.7654 -name clk_cpu -waveform {0 49.3827} [get_nets {clk_cpu}]

create_clock -period 8.000 -name clk125 [get_nets {clk125}]
set_false_path -from [get_clocks {clk125}] -to [get_clocks {sys_clk}]
