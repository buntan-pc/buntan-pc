TARGET = ./obj_dir/Vsim_top

SV = $(addprefix ../, \
	common.sv \
	cpu.sv \
	mem.sv \
	pmem_sim.sv \
	alu.sv \
	stack.sv \
	signals.sv \
	cdtimer.sv \
	signalizer.sv \
	decoder.sv \
	mcu.sv \
	uart.sv \
	uart_mux.sv \
	adc.sv \
	simple_timer.sv \
	spi.sv \
	kbc.sv \
	i2c.sv) \
	sim_top.sv

.PHONY: all
all: $(TARGET)

.PHONY: run
run: $(TARGET)
	./obj_dir/Vsim_top +pmem=../../ucc/testcase.pmem.hex +dmem=../../ucc/testcase.dmem.hex

$(TARGET): $(SV) sim_main.cpp
	verilator \
		-Wall -Wno-fatal \
		-O3 \
		--cc --exe --build -j 0 \
		--top-module sim_top \
		--public-flat-rw \
		-CFLAGS "-std=c++17 -DVL_VERILATED_INCLUDE=\\\"../verilator_stderr.h\\\"" \
		-I.. \
		$^
