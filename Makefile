CFLAGS 			 = -g -Wall -std=c2x -fPIC
BUILD_PREFIX = build
SRC_PREFIX 	 = src/**
INCLUDE			 = -Iinclude
LD_CGAB			 = -L$(BUILD_PREFIX) -lcgab

OS_SRC = $(wildcard src/os/*.c)
OS_OBJ = $(OS_SRC:src/os/%.c=$(BUILD_PREFIX)/%.o)

CGAB_SRC = $(wildcard src/gab/*.c)
CGAB_OBJ = $(CGAB_SRC:src/gab/%.c=$(BUILD_PREFIX)/%.o)

CLI_SRC = $(wildcard src/cli/*.c)
CLI_OBJ = $(CLI_SRC:src/cli/%.c=$(BUILD_PREFIX)/%.o)

$(echo $(CLI_SRC))

all: $(BUILD_PREFIX)/cli

$(BUILD_PREFIX)/cli: $(CLI_OBJ) $(BUILD_PREFIX)/libcgab
	$(CC) $(CFLAGS) $(INCLUDE) $(LD_CGAB) $(CLI_OBJ) -o $@

$(BUILD_PREFIX)/libcgab: $(OS_OBJ) $(CGAB_OBJ)
	$(CC) $(CFLAGS) $(INCLUDE) $(CGAB_OBJ) $(OS_OBJ) --shared -o $@

$(BUILD_PREFIX)/%.o: $(SRC_PREFIX)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -MMD $< -c -o $@

.PHONY: clean
clean:
	rm $(BUILD_PREFIX)/*
