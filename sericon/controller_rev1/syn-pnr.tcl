set_device -name "GW1NR-9C" "GW1NR-LV9QN88PC6/I5"

set_option -verilog_std sysv2017
set_option -print_all_synthesis_warning 1
set_option -top_module "controller_top"
set_option -output_base_name "controller"
set_option -use_sspi_as_gpio 1

add_file "src/port.cst"
add_file "src/timing.sdc"
add_file "src/ip/gowin_rpll_fclk/gowin_rpll_fclk.v"

set flist [open src/controller.f r]
while {[gets $flist line] >= 0} {
  add_file $line
}

run all
