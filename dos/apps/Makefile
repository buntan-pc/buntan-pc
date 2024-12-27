CC = ../../ucc/cc
ASM = ../../uas/as

%.pmem.hex: %.s Makefile
	$(ASM) --pmem $@ $<

%.dmem.hex: %.s Makefile
	$(ASM) --dmem $@ $<

%.exe: %.s Makefile
	$(ASM) -o $@ $<

%.s: %.c Makefile
	$(CC) --ret-from-start -o $@ $<

.PHONY: clean
clean:
	rm -f *.s *.exe *.pmem.hex *.dmem.hex *.map
