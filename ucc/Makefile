OBJS   = main.o ast.o insn.o symbol.o token.o type.o
CFLAGS = -Wall -Wextra -O0 -g

.PHONY: all
all: cc

cc: $(OBJS)
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS)

.PHONY: test e2etest
test: cc
	./test.sh

e2etest: cc ../uas/as ../cpu/sim.exe
	./e2etest.sh

DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

.%.d: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

-include $(DEPENDS)
