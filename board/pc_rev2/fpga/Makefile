GW_SH := ./gw_sh.exe
#GW_SH := ./gw_sh_1.9.8.03.exe

PROJ_BASE_NAME := pc_rev2
TARGET := impl/pnr/$(PROJ_BASE_NAME).fs
SRC_LIST := src/$(PROJ_BASE_NAME).f
RESOURCE_REPORT := impl/pnr/$(PROJ_BASE_NAME).rpt.html
RESOURCE_HISTORY := resource-history.json

CPU_SRC_DIR := ../../../cpu
SRCS := src/port.cst src/timing.sdc src/main.sv \
	src/gowin_osc/gowin_osc.v \
	src/gowin_rpll/gowin_rpll_20mhz.v \
	src/gowin_rpll/gowin_rpll_10mhz.v \
	$(addprefix $(CPU_SRC_DIR)/, \
	sim.sv cpu.sv mem.sv pmem_gowin.sv alu.sv stack.sv signals.sv cdtimer.sv \
	signalizer.sv decoder.sv mcu_rev2.sv uart.sv uart_mux.sv adc_rev2.sv \
	simple_timer.sv spi.sv kbc.sv i2c.sv pmem_init.sv)

all: $(TARGET)
	@./resource_usage.py $(RESOURCE_REPORT) $(RESOURCE_HISTORY)

$(TARGET): $(SRCS) $(SRC_LIST) syn-pnr.tcl Makefile
	@$(GW_SH) < syn-pnr.tcl 2>&1 | { \
		err=0; \
		while IFS= read -r line; do \
		    /usr/bin/echo "$$line"; \
		    /usr/bin/echo "$$line" | grep -q "^ERROR" && err=1; \
		done; \
		[ $$err -eq 0 ] || exit 1; \
	}

.PHONY: all FORCE

$(CPU_SRC_DIR)/pmem_init.sv: FORCE
	make -C $(CPU_SRC_DIR) pmem_init.sv

# $(file >$@) で SRC_LIST を空にしてから、
# SRCS の各要素を 1 行ずつ書き出す。
$(SRC_LIST): $(SRCS) Makefile
	@$(file >$@)
	@$(foreach src,$(SRCS),$(file >>$@,$(src)))
