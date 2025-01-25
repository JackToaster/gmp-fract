CC=tcc -Wunsupported

DEBUG_FLAGS=-Wall -g -b
RELEASE_FLAGS=-Wall -Wpedantic -O3 

DBG ?= 1
ifeq ($(DBG), 1)
CFLAGS = $(DEBUG_FLAGS)
$(info Debug enabled)
else
CC=gcc
CFLAGS = $(RELEASE_FLAGS)
endif

SRC_DIR=src
BUILD_DIR=build
TARGET=gmpfract

# check if source directory exists
ifeq ($(wildcard $(SRC_DIR)),)
$(error Source directory '$(SRC_DIR)' not found.)
endif
# Check if the target exist in the src directory
ifeq ($(wildcard $(SRC_DIR)/$(TARGET).c),)
$(error Target source file '$(TARGET).c' not found in $(SRC_DIR).)
endif

SRC_FILES := $(wildcard $(SRC_DIR)/**/*.c) $(wildcard $(SRC_DIR)/*.c)

OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))

DEP_FILES := $(OBJ_FILES:.o=.d)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

-include $(DEP_FILES)


all: $(TARGET)

run: $(TARGET)
	$(info ------------------------------------------)
	./$(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) -o $@$(BIN_EXT) $^ $(CFLAGS)

.PHONY: clean

# Help message
define HELP_MESSAGE
Usage: make [target]\n
Targets:
	all            - Build the main target (default).
	debug          - Build the main target with debug symbols. Uses -g flag (default), this lets you use gdb to debug the executable.
	clean          - Remove built files.
	help           - Display this help message.\n\n
endef
export HELP_MESSAGE

clean:
	rm -rf $(BUILD_DIR) $(TARGET)$(BIN_EXT)
