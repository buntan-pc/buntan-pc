set_device -name "GW1NR-9C" "GW1NR-LV9QN88PC6/I5"

set_option -verilog_std sysv2017
set_option -print_all_synthesis_warning 1
set_option -top_module "main"
set_option -output_base_name "pc_rev1"
set_option -use_sspi_as_gpio 1

set flist [open src/pc_rev1.f r]
while {[gets $flist line] >= 0} {
  add_file $line
}
close $flist

run all
