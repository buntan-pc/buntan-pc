CC      = gcc
CXX     = g++
CFLAGS  = -Wall -Wextra -std=c99
CXXFLAGS= -Wall -Wextra -std=c++17
OUT     = out
BIN     = bemu-system-buntan_pc

VERILATOR_DIR = cpu
VERILATOR_TOP = Vcpu

all: getsrc verilator build_verilator
	@echo "Compiling C source files..."
	@mkdir -p $(OUT)
	@while read src; do \
		obj=$$(basename $$src .c).o; \
		echo "Compiling $$src -> $(OUT)/$$obj"; \
		$(CC) $(CFLAGS) -c $$src -o $(OUT)/$$obj; \
	done < .index

	@echo "Compiling api.cpp..."
	@$(CXX) $(CXXFLAGS) \
		-I$(VERILATOR_DIR) \
		-I/usr/share/verilator/include \
		-I/usr/share/verilator/include/vltstd \
		-c api.cpp -o $(OUT)/api.o

	@echo "Linking object files..."
	@$(CXX) $(CXXFLAGS) \
		-o $(OUT)/$(BIN) \
		$(OUT)/*.o \
		$(VERILATOR_DIR)/$(VERILATOR_TOP)__ALL.a

	@rm .index

build: all

run: all
	@echo "Running emulator..."
	@./$(OUT)/$(BIN)

getsrc:
	@echo "Getting source files..."
	@find . -name "*.c" > .index

verilator:
	@echo "Converting Verilog to cpp..."
	@verilator --cc ../cpu/cpu.sv \
		--top-module cpu \
		-I../cpu \
		--Mdir $(VERILATOR_DIR) \
		-Wno-fatal

build_verilator:
	@echo "Building Verilated CPU..."
	@$(MAKE) -C $(VERILATOR_DIR) -f $(VERILATOR_TOP).mk

clean:
	@echo "Cleaning up..."
	@rm -rf $(OUT) .index $(VERILATOR_DIR)

.PHONY: all build run getsrc clean verilator build_verilator
.DEFAULT_GOAL := all