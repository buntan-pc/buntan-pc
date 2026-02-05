GWPROG := C:\Gowin\Gowin_V1.9.12.01_x64\Programmer\bin\programmer_cli.exe
GW_DEV := GW1NR-9C

TARGET := impl/pnr/controller.fs

SRCS := src/port.cst \
	src/timing.sdc \
	src/ip/gowin_rpll_fclk/gowin_rpll_fclk.v \
	src/veryl/top.veryl

VERYL_SRCS := $(filter %.veryl,$(SRCS))

all: $(TARGET)

$(TARGET): $(SRCS) src/controller.f src/hankaku.hex src/scrn_init.hex Makefile
	@./gw_sh.exe < syn-pnr.tcl 2>&1 | { \
		err=0; \
		while IFS= read -r line; do \
		    /usr/bin/echo "$$line"; \
		    /usr/bin/echo "$$line" | grep -q "^ERROR" && err=1; \
		done; \
		[ $$err -eq 0 ] || exit 1; \
	}

src/controller.f: $(VERYL_SRCS) Makefile
	rm -f $@
	cd src && veryl build

src/hankaku.hex: src/hankaku.txt Makefile
	./makefont.py -o $@ $<

.PHONY: sram flash
sram: $(TARGET) Makefile
	./gwprog.py sram $(GW_DEV) '$(shell wslpath -a -w $(TARGET))'

flash: $(TARGET) Makefile
	./gwprog.py flash $(GW_DEV) '$(shell wslpath -a -w $(TARGET))'
