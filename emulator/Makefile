CC		= gcc
CFLAGS	= -Wall -Wextra -std=c99
OUT		= out
BIN		= bemu-system-buntan_pc

all: getsrc verilator
	@echo "Compiling source files..."
	@mkdir -p $(OUT)
	@while read src; do \
		obj=$$(basename $$src .c).o; \
		echo "Compiling $$src -> $(OUT)/$$obj"; \
		$(CC) $(CFLAGS) -c $$src -o $(OUT)/$$obj; \
	done < .index
	@echo "Linking object files..."
	@$(CC) $(CFLAGS) -o $(OUT)/$(BIN) $(OUT)/*.o
	@rm .index

build: all

run: all
	@echo "Running emulator..."
	@./$(OUT)/$(BIN)

getsrc:
	@echo "Getting source files..."
	@find . -name "*.c" > .index

clean:
	@echo "Cleaning up..."
	@rm -rf $(OUT) .index

verilator:
	@echo "Converting Verilog to cpp..."
	@verilator --cc ../cpu/cpu.sv \
		--top-module cpu \
		-I../cpu \
		--Mdir cpu \
		-Wno-fatal \

.PHONY: all build run getsrc clean verilator
.DEFAULT_GOAL := all