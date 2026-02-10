CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Isrc/include
ASFLAGS = -fno-pic -no-pie

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

XLD     = xld

SRC_DIR = src
SRC     = $(SRC_DIR)/main.c $(SRC_DIR)/include/options.c
OBJ     = $(SRC:.c=.o)

EXAMPLES_DIR = examples
EXAMPLES_SRC = $(wildcard $(EXAMPLES_DIR)/*.S)
EXAMPLES_OBJ = $(EXAMPLES_SRC:.S=.o)
EXAMPLES_BIN = $(EXAMPLES_SRC:.S=.out)

.PHONY: all build install uninstall clean examples run-examples

all: build

build: $(XLD)

$(XLD): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: build
	install -Dm755 $(XLD) $(BINDIR)/$(XLD)
	@echo "xld installed in: $(BINDIR)"

uninstall:
	rm -f $(BINDIR)/$(XLD)
	@echo "xld removed from: $(BINDIR)"

examples: build $(EXAMPLES_BIN)

$(EXAMPLES_DIR)/%.o: $(EXAMPLES_DIR)/%.S
	$(CC) -c $< -o $@ $(ASFLAGS)

$(EXAMPLES_DIR)/%.out: $(EXAMPLES_DIR)/%.o
	./$(XLD) -o $@ $<
	chmod +x $@

run-examples: examples
	@for bin in $(EXAMPLES_BIN); do \
		echo ">>> Running $$bin"; \
		$$bin || true; \
		echo ""; \
	done

clean:
	rm -f $(XLD)
	rm -f $(OBJ)
	rm -f $(EXAMPLES_DIR)/*.o
	rm -f $(EXAMPLES_DIR)/*.out

