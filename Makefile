# Secure Code Judge Sandbox
# ต้องติดตั้งก่อน: sudo apt install build-essential libseccomp-dev strace

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=gnu11 -Iinclude
LDLIBS  := -lseccomp

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)
BIN     := judge

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c include/sandbox.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

test: $(BIN)
	bash scripts/run_all_tests.sh

demo: $(BIN)
	python3 web/server.py

clean:
	rm -rf build $(BIN)

.PHONY: all test demo clean
