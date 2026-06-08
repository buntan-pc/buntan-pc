TARGET = ./obj_dir/Vsim_top
CLOCK_HZ = 1000000
INSN_DEF = ../insn_names.def
INSN_GEN = ../gen_insn_names.py
INSN_CPP_INC = insn_names_cpp.inc

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

$(INSN_CPP_INC): $(INSN_DEF) $(INSN_GEN)
	python3 $(INSN_GEN) cpp $(INSN_DEF) > $@

$(TARGET): $(SV) sim_main.cpp $(INSN_CPP_INC) Makefile
	verilator \
		-Wall -Wno-fatal \
		-O3 \
		--cc --exe --build -j 0 \
		--top-module sim_top \
		-GCLOCK_HZ=$(CLOCK_HZ) \
		--public-flat-rw \
		-CFLAGS "-std=c++17 -DSIM_CLOCK_HZ=$(CLOCK_HZ) -DVL_VERILATED_INCLUDE=\\\"../verilator_stderr.h\\\"" \
		-I.. \
		$(SV) sim_main.cpp

.PHONY: clean
clean:
	rm -f $(INSN_CPP_INC)
