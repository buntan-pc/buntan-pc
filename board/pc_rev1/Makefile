GW_SH := ./gw_sh.exe
#GW_SH := ./gw_sh_1.9.8.03.exe

TARGET := impl/pnr/pc_rev1.fs
SRC_LIST := src/pc_rev1.f

SRCS := src/port.cst src/timing.sdc src/main.sv \
	src/gowin_osc/gowin_osc.v \
	src/gowin_rpll/gowin_rpll_20mhz.v \
	src/gowin_rpll/gowin_rpll_10mhz.v \
	$(addprefix ../../cpu/, \
	sim.sv cpu.sv mem.sv alu.sv stack.sv signals.sv cdtimer.sv \
	signalizer.sv decoder.sv mcu.sv uart.sv uart_mux.sv adc.sv \
	simple_timer.sv spi.sv kbc.sv i2c.sv pmem_init.sv)

all: $(TARGET)

$(TARGET): $(SRCS) $(SRC_LIST) Makefile
	@$(GW_SH) < syn-pnr.tcl 2>&1 | { \
		err=0; \
		while IFS= read -r line; do \
		    /usr/bin/echo "$$line"; \
		    /usr/bin/echo "$$line" | grep -q "^ERROR" && err=1; \
		done; \
		[ $$err -eq 0 ] || exit 1; \
	}

.PHONY: FORCE

../../cpu/pmem_init.sv: FORCE
	make -C ../../cpu pmem_init.sv

# $(file >$@) で SRC_LIST を空にしてから、
# SRCS の各要素を 1 行ずつ書き出す。
$(SRC_LIST): $(SRCS) Makefile
	@$(file >$@)
	@$(foreach src,$(SRCS),$(file >>$@,$(src)))
