CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -g
LDFLAGS :=
LDLIBS  :=

SRC     := main.c arena.c token.c
OBJ     := $(SRC:.c=.o)
BIN     := mgcc

TEST_DIR    := tests
TEST_SCRIPT := $(TEST_DIR)/compile_test.sh
TEST_OUTDIR := $(TEST_DIR)/build

TEST_SRCS := $(wildcard $(TEST_DIR)/t[0-9][0-9][0-9]_*.c)

.PHONY: all clean run test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN) --help

# Compile and run each test file, printing a per-file PASS/FAIL status,
# then a summary at the end. Continues past individual failures.
test: $(BIN)
	@mkdir -p $(TEST_OUTDIR)
	@total=0; pass=0; fail=0; \
	for src in $(TEST_SRCS); do \
	    total=$$((total + 1)); \
	    name=`basename "$$src" .c`; \
	    flags=""; \
	    case "$$name" in \
	        t003_header) flags="-Itests/inc";; \
	    esac; \
	    if $(TEST_SCRIPT) "$$src" $$flags; then \
	        pass=$$((pass + 1)); \
	    else \
	        fail=$$((fail + 1)); \
	    fi; \
	done; \
	echo "=========================================="; \
	echo "Summary"; \
	echo "=========================================="; \
	echo "Total: $$total"; \
	echo "Pass:  $$pass"; \
	echo "Fail:  $$fail"; \
	echo "Result: $$( [ $$fail -eq 0 ] && echo 'ALL PASS' || echo 'FAILURES' )"; \
	[ $$fail -eq 0 ]

clean:
	rm -f $(OBJ) $(BIN)
	rm -rf $(TEST_OUTDIR)
