CC 		 = gcc
CFLAGS = -std=c2x -fPIC -foptimize-sibling-calls -MMD -Wall -DGAB_PREFIX=\"${GAB_PREFIX}\" ${GAB_CCFLAGS}

SRC_PREFIX 	 	 	= src/**
BUILD_PREFIX 	 	= build
INCLUDE_PREFIX 	= include

INCLUDE	= -I$(INCLUDE_PREFIX)
LD_CGAB	= -L$(BUILD_PREFIX) -L/usr/lib -lcgab 

OS_SRC = $(wildcard src/os/*.c)
OS_OBJ = $(OS_SRC:src/os/%.c=$(BUILD_PREFIX)/%.o)

CGAB_SRC = $(wildcard src/cgab/*.c)
CGAB_OBJ = $(CGAB_SRC:src/cgab/%.c=$(BUILD_PREFIX)/%.o)

GAB_SRC = $(wildcard src/gab/*.c)
GAB_OBJ = $(GAB_SRC:src/gab/%.c=$(BUILD_PREFIX)/%.o)

MOD_SRC 	 = $(wildcard src/mod/*.c)
MOD_SHARED = $(MOD_SRC:src/mod/%.c=$(BUILD_PREFIX)/libcgab%.so)

STD_SRC = $(wildcard std/*.gab)

all: $(BUILD_PREFIX)/gab modules

-include $(OS_OBJ:.o=.d) $(CGAB_OBJ:.o=.d) $(GAB_OBJ:.o=.d) $(MOD_SHARED:.so=.d)

modules: $(MOD_SHARED)

$(BUILD_PREFIX)/gab: $(GAB_OBJ) $(BUILD_PREFIX)/libcgab.so
	$(CC) $(CFLAGS) $(INCLUDE) $(LD_CGAB) $(GAB_OBJ) -o $@

$(BUILD_PREFIX)/libcgab.so: $(OS_OBJ) $(CGAB_OBJ)
	$(CC) $(CFLAGS) $(INCLUDE) $(CGAB_OBJ) $(OS_OBJ) --shared -o $@

$(BUILD_PREFIX)/%.o: $(SRC_PREFIX)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) $< -c -o $@

$(BUILD_PREFIX)/libcgab%.so: $(SRC_PREFIX)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) $(LD_CGAB) $< --shared -o $@

INSTALL_PREFIX 	= ${GAB_INSTALLPREFIX}
GAB_PATH 			= ${GAB_PREFIX}/gab

install_dev:
	install -vCDt $(INSTALL_PREFIX)/include/gab $(INCLUDE_PREFIX)/*

install_std:
	install -CDt $(GAB_PATH)/modules $(STD_SRC)

install_modules: modules
	install -vCDt $(GAB_PATH)/modules $(MOD_SHARED)

install_gab: $(BUILD_PREFIX)/gab
	install -vC $(BUILD_PREFIX)/gab $(INSTALL_PREFIX)/bin
	install -vC $(BUILD_PREFIX)/libcgab.so $(INSTALL_PREFIX)/lib

uninstall:
	rm -rf $(INSTALL_PREFIX)/include/gab
	rm $(INSTALL_PREFIX)/lib/libcgab.so
	rm $(INSTALL_PREFIX)/bin/gab

compile_commands:
	make clean
	bear -- make

.PHONY: clean
clean:
	rm -vf $(BUILD_PREFIX)/*
