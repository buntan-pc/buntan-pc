OBJS   = kuas.o
CFLAGS = -Wall -Wextra -O0 -g

.PHONY: all
all: as

as: $(OBJS)
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS)
