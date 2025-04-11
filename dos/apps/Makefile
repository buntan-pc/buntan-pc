CC = ../../ucc/cc
ASM = ../../uas/as
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

.PHONY: clean
clean:
	rm -f *.s *.exe *.pmem.hex *.dmem.hex *.map
