CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -g
LDFLAGS :=
LDLIBS  :=

SRC     := main.c
OBJ     := $(SRC:.c=.o)
BIN     := mgcc

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --help

clean:
	rm -f $(OBJ) $(BIN)
