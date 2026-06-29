CC = ../../ucc/cc
ASM = ../../uas/as
CC_LINUX = gcc
SRCS = $(filter-out buntan_emu.c,$(wildcard *.c))
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

%.lst: %.s Makefile
	$(ASM) --list $@ -o $*.exe $<

# for emulation on Linux
buntan_emu.o: buntan_emu.c Makefile
	$(CC_LINUX) -g -mx32 -c -DDEF_MAIN -o $@ $<

%.elf: %.c buntan_emu.o Makefile
	$(CC_LINUX) -g -mx32 -c \
		-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-attributes \
		-include stdarg.h -include buntan_emu.h \
		-o $*.o $<
	$(CC_LINUX) -g -mx32 -o $@ $*.o buntan_emu.o

.PHONY: clean
clean:
	rm -f *.s *.exe *.pmem.hex *.dmem.hex *.map
