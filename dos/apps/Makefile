CC = ../../ucc/cc
ASM = ../../uas/as
CC_LINUX = gcc
SRCS = $(wildcard *.c)
EXES = $(SRCS:.c=.exe)

all: $(EXES) Makefile

%.pmem.hex: %.s Makefile
	$(ASM) --pmem $@ $<

%.dmem.hex: %.s Makefile
	$(ASM) --dmem $@ $<

%.exe: %.s Makefile
	$(ASM) -o $@ $<

%.s: %.c Makefile
	$(CC) --ret-from-start --offset-global-addr 4 -o $@ $<

# for emulation on Linux
%.elf: %.c buntan_emu.c Makefile
	$(CC_LINUX) -mx32 -c -Dmain=buntan_main -Wno-int-conversion -Wno-incompatible-pointer-types -o $*.o $<
	$(CC_LINUX) -mx32 -o $@ $*.o buntan_emu.c

.PHONY: clean
clean:
	rm -f *.s *.exe *.pmem.hex *.dmem.hex *.map
