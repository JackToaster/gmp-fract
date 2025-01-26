CC=clang

FLAGS=-Wall -MP -MD
DEBUG_FLAGS=$(FLAGS) -O1 -g
RELEASE_FLAGS=$(FLAGS) -O3 


DBG ?= 1
ifeq ($(DBG), 1)
CFLAGS = $(DEBUG_FLAGS)
$(info Debug enabled)
else
CFLAGS = $(RELEASE_FLAGS)
endif

SRC_DIR=src
INC_DIR=inc
BUILD_DIR=build
TARGET=gmpfract

INC=-I$(INC_DIR)
LIB=-l:libraylib.so.550 -lgmp -lm -lpthread
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
	$(CC) $(INC) -c -o $@ $< $(CFLAGS)


all: $(TARGET)

-include $(DEP_FILES)

run: $(TARGET)
	$(info ------------------------------------------)
	./$(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC)  $(INC) -o $@$(BIN_EXT) $^ $(CFLAGS) $(LIB) 

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
