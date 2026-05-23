CC      = gcc
CXX     = g++
CFLAGS  = -Wall -Wextra -std=c99
CXXFLAGS= -Wall -Wextra -std=c++17
OUT     = out
BIN     = bemu-system-buntan_pc

VERILATOR_DIR = cpu
VERILATOR_TOP = Vcpu

VERILATOR_INC = /usr/share/verilator/include
VERILATOR_VLTSTD = /usr/share/verilator/include/vltstd

SRCS_C = $(wildcard *.c)
OBJS_C = $(patsubst %.c,$(OUT)/%.o,$(SRCS_C))
OBJ_API = $(OUT)/api.o

all: verilator build_verilator $(OUT)/$(BIN)

build: all

run: all
	@echo "Running emulator..."
	@./$(OUT)/$(BIN)

$(OUT):
	@mkdir -p $(OUT)

verilator:
	@echo "Converting Verilog to cpp..."
	@verilator -sv --cc ../cpu/cpu.sv \
		--top-module cpu \
		-I../cpu \
		--Mdir $(VERILATOR_DIR) \
		-Wno-fatal

build_verilator:
	@echo "Building Verilated CPU..."
	@$(MAKE) -C $(VERILATOR_DIR) -f $(VERILATOR_TOP).mk

$(OUT)/%.o: %.c | $(OUT)
	@echo "Compiling $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_API): api.cpp api.h | $(OUT)
	@echo "Compiling api.cpp -> $@"
	@$(CXX) $(CXXFLAGS) \
		-I$(VERILATOR_DIR) \
		-I$(VERILATOR_INC) \
		-I$(VERILATOR_VLTSTD) \
		-c api.cpp -o $@

$(OUT)/$(BIN): $(OBJS_C) $(OBJ_API)
	@echo "Linking -> $@"
	@$(CXX) $(CXXFLAGS) -pthread \
		-o $@ \
		$(OBJS_C) $(OBJ_API) \
		$(VERILATOR_DIR)/libVcpu.a \
		$(VERILATOR_DIR)/libverilated.a

clean:
	@echo "Cleaning up..."
	@rm -rf $(OUT) $(VERILATOR_DIR)

.PHONY: all build run clean verilator build_verilator
.DEFAULT_GOAL := all
