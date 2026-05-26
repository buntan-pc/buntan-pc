CC      = gcc
CXX     = g++
CFLAGS  = -Wall -Wextra -std=c99
CXXFLAGS= -Wall -Wextra -std=c++17
OUT     = out
BIN     = bemu-system-buntan_pc
CONFIG  = .config

-include $(CONFIG)

ifeq ($(CONFIG_DEBUG),y)
CFLAGS += -DDEBUG
CXXFLAGS += -DDEBUG
endif
ifeq ($(CONFIG_IMAGE_FILE),)
IMAGE_FILE = out/sd.img
else
IMAGE_FILE = $(CONFIG_IMAGE_FILE)
endif

SD_IMAGE = $(IMAGE_FILE)

CFLAGS += -DIMAGE_FILE=\"$(IMAGE_FILE)\"
CXXFLAGS += -DIMAGE_FILE=\"$(IMAGE_FILE)\"

VERILATOR_DIR = mcu
VERILATOR_TOP = Vmcu

VERILATOR_DIR_MCU = mcu
VERILATOR_TOP_MCU = Vmcu

VERILATOR_INC = /usr/share/verilator/include
VERILATOR_VLTSTD = /usr/share/verilator/include/vltstd

DOS_APPS_DIR = ../dos/apps

SRCS_C = $(wildcard *.c)
OBJS_C = $(patsubst %.c,$(OUT)/%.o,$(SRCS_C))
OBJ_API = $(OUT)/api.o
OBJ_VMCU = $(OUT)/vmcu_main.o

all: verilator build_verilator $(OUT)/$(BIN)

build: all

run: $(SD_IMAGE) all fmt
	@echo "Running emulator..."
	@./$(OUT)/$(BIN)

vmcu: verilator_mcu build_verilator_mcu $(OUT)/bemu-system-buntan_mcu
	@echo "Running Vmcu..."
	@./$(OUT)/bemu-system-buntan_mcu

$(OUT):
	@mkdir -p $(OUT)

verilator:
	@echo "Converting Verilog to cpp..."
	@verilator -sv --cc \
		-GCLOCK_HZ=200000 \
		-GUART_BAUD=1000 \
		../cpu/mcu.sv \
		../cpu/cpu.sv \
		../cpu/mem.sv \
		../cpu/alu.sv \
		../cpu/stack.sv \
		../cpu/signals.sv \
		../cpu/signalizer.sv \
		../cpu/decoder.sv \
		../cpu/uart.sv \
		../cpu/uart_mux.sv \
		../cpu/simple_timer.sv \
		../cpu/spi.sv \
		../cpu/kbc.sv \
		../cpu/i2c.sv \
		../cpu/adc.sv \
		../cpu/cdtimer.sv \
		--top-module mcu \
		-I../cpu \
		--Mdir $(VERILATOR_DIR) \
		-Wno-fatal

verilator_mcu:
	@echo "Converting MCU Verilog to cpp..."
	@verilator -sv --cc ../cpu/*.sv \
		--top-module mcu \
		-I../cpu \
		--Mdir $(VERILATOR_DIR_MCU) \
		-Wno-fatal

build_verilator:
	@echo "Building Verilated CPU..."
	@$(MAKE) -C $(VERILATOR_DIR) -f $(VERILATOR_TOP).mk

build_verilator_mcu:
	@echo "Building Verilated MCU..."
	@$(MAKE) -C $(VERILATOR_DIR_MCU) -f $(VERILATOR_TOP_MCU).mk

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

$(OBJ_VMCU): vmcu_main.cpp | $(OUT)
	@echo "Compiling vmcu_main.cpp -> $@"
	@$(CXX) $(CXXFLAGS) -I$(VERILATOR_DIR_MCU) -I$(VERILATOR_INC) -I$(VERILATOR_VLTSTD) -c vmcu_main.cpp -o $@

$(OUT)/$(BIN): $(OBJS_C) $(OBJ_API)
	@echo "Linking -> $@"
	@$(CXX) $(CXXFLAGS) -pthread \
		-o $@ \
		$(OBJS_C) $(OBJ_API) \
		$(VERILATOR_DIR)/libVmcu.a \
		$(VERILATOR_DIR)/libverilated.a

$(OUT)/bemu-system-buntan_mcu: $(OBJ_VMCU)
	@echo "Linking Vmcu -> $@"
	@$(CXX) $(CXXFLAGS) -pthread -o $@ $(OBJ_VMCU) $(VERILATOR_DIR_MCU)/libVmcu.a $(VERILATOR_DIR_MCU)/libverilated.a

fmt:
	@echo "Formatting..."
	@clang-format -i *.c *.cpp *.h

apps:
	@echo "Building DOS apps..."
	@$(MAKE) -C $(DOS_APPS_DIR)

$(SD_IMAGE): apps
	@echo "Creating MBR + FAT16 image..."
	@rm -f $(SD_IMAGE)
	@dd if=/dev/zero of=$(SD_IMAGE) bs=512 count=8192 status=none
	@parted -s $(SD_IMAGE) mklabel msdos
	@parted -s $(SD_IMAGE) unit s mkpart primary fat16 1 8191
	@mformat -i $(SD_IMAGE)@@512 -F ::
	@mcopy -i $(SD_IMAGE)@@512 $(DOS_APPS_DIR)/*.exe ::

clean:
	@echo "Cleaning up..."
	@rm -rf $(OUT) $(VERILATOR_DIR) $(VERILATOR_DIR_MCU)

.PHONY: all build run fmt clean verilator build_verilator verilator_mcu build_verilator_mcu vmcu apps
.DEFAULT_GOAL := all
