CC 		 = zig cc
CFLAGS = -std=c2x --target=$(TARGET) -fPIC -Wall -DGAB_PREFIX=\"${GAB_PREFIX}\" ${GAB_CCFLAGS}

SRC_PREFIX 	 	 	= src/**
BUILD_PREFIX 	 	= build-$(TARGET)
INCLUDE_PREFIX 	= include
VENDOR_PREFIX   = vendor

INCLUDE	= -I$(INCLUDE_PREFIX) -I$(VENDOR_PREFIX)
LD_CGAB	= $(BUILD_PREFIX)/libcgab.a

OS_SRC = $(wildcard src/os/*.c)
OS_OBJ = $(OS_SRC:src/os/%.c=$(BUILD_PREFIX)/%.o)

CGAB_SRC = $(wildcard src/cgab/*.c)
CGAB_OBJ = $(CGAB_SRC:src/cgab/%.c=$(BUILD_PREFIX)/%.o)

GAB_SRC = $(wildcard src/gab/*.c)
GAB_OBJ = $(GAB_SRC:src/gab/%.c=$(BUILD_PREFIX)/%.o)

CMOD_SRC 	 = $(wildcard src/mod/*.c)
CMOD_SHARED = $(CMOD_SRC:src/mod/%.c=$(BUILD_PREFIX)/libcgab%.so)

GMOD_SRC = $(wildcard mod/*.gab)
GABLSP_SRC = $(wildcard gab.lsp/*.gab)

all: $(BUILD_PREFIX)/gab cmodules

-include $(OS_OBJ:.o=.d) $(CGAB_OBJ:.o=.d) $(GAB_OBJ:.o=.d) $(CMOD_SHARED:.so=.d)

cmodules: $(CMOD_SHARED)

$(BUILD_PREFIX)/gab: $(GAB_OBJ) $(BUILD_PREFIX)/libcgab.a
	$(CC) $(CFLAGS) $(INCLUDE) $(GAB_OBJ) $(LD_CGAB) -o $@

$(BUILD_PREFIX)/libcgab.a: $(OS_OBJ) $(CGAB_OBJ)
	ar rcs $@ $^

$(BUILD_PREFIX)/%.o: $(SRC_PREFIX)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) $< -c -o $@

$(BUILD_PREFIX)/libcgab%.so: $(SRC_PREFIX)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) $< $(LD_CGAB) --shared -o $@

INSTALL_PREFIX 	= ${GAB_INSTALLPREFIX}
GAB_PATH 			= ${GAB_PREFIX}/gab

# install_dev:
# 	install -vCDt $(INSTALL_PREFIX)/include/gab $(INCLUDE_PREFIX)/*

# install_gab: $(BUILD_PREFIX)/gab
# 	install -vC $(BUILD_PREFIX)/gab $(INSTALL_PREFIX)/bin
# 	install -vC $(BUILD_PREFIX)/libcgab.a $(INSTALL_PREFIX)/lib

# install_gab.lsp:
# 	install -CDt $(GAB_PATH)/modules/gab.lsp $(GABLSP_SRC)

# install_modules: cmodules
# 	install -vCDt $(GAB_PATH)/modules $(CMOD_SHARED)
# 	install -CDt $(GAB_PATH)/modules $(GMOD_SRC)

uninstall:
	rm -rf $(INSTALL_PREFIX)/include/gab
	rm $(INSTALL_PREFIX)/lib/libcgab.so
	rm $(INSTALL_PREFIX)/bin/gab

test: $(BUILD_PREFIX)/gab modules
	$(BUILD_PREFIX)/gab run test

clean:
	rm -rf build-*/

compile_commands:
	mkdir -p $(BUILD_PREFIX)
	bear -- make TARGET=$(TARGET)

.PHONY: clean
