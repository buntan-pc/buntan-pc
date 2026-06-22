CC_LINUX = gcc

all: test

test_printf: test_printf.c ../buntan_emu.c ../printf.h ../syscall.h Makefile
	$(CC_LINUX) -g -mx32 -DNOMAIN -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-builtin-declaration-mismatch -o $@ $(filter %.c,$^)

.PHONY: test
test: test_printf
	./test_printf
