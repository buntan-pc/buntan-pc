CC = ../../compiler/uchan/cc
ASM = ../../assembler/uchan/as

all: Makefile
	make -r dos.exe

dos.dmem.hex: dos.hexfile
dos.pmem.hex: dos.hexfile
%.hexfile: %.s Makefile
	$(ASM) --pmem $*.pmem.hex --dmem $*.dmem.hex $<

%.exe: %.s Makefile
	$(ASM) -o $@ $<

%.s: %.c Makefile
	$(CC) -o $@ $<

.PHONY: run clean
run: dos.pmem.hex dos.dmem.hex
	../../tool/uart.py --pmem dos.pmem.hex --dmem dos.dmem.hex --nowait
clean:
	rm -f dos.s dos.exe dos.pmem.hex dos.dmem.hex dos.map
