CC      := gcc
CFLAGS  := -Wall -Wextra -std=c99 -O2 -Iinclude
LDFLAGS :=

SRC     := src/common.c src/account.c src/wal.c src/transaction.c src/ledger.c
OBJ     := $(SRC:src/%.c=build/%.o)
TARGET  := build/ledger
TEST_TARGET := build/test_ledger

.PHONY: all clean run test

all: $(TARGET)

build:
	@mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ) build/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/main.o: src/main.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_TARGET): $(OBJ) build/test_ledger.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/test_ledger.o: tests/test_ledger.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
