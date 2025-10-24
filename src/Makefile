# Makefile for the 'src' directory (assumes it's in a project root/src structure)

# --- Configuration ---

# Directories
# BUILD_DIR is now defined relative to the Makefile's location (one level up)
PROJECT_ROOT := ..
SRC_DIR := bTree
BUILD_DIR := $(PROJECT_ROOT)/build
IMPL_DIR := $(SRC_DIR)/implements
HEADERS_DIR := $(SRC_DIR)/headers
TEST_FILE := $(SRC_DIR)/test.cpp

# Target Names
LIB_NAME := libbtree.a
TEST_EXEC := btree_test

# Determine the build type (default to debug)
# Usage: make BUILD=release or make BUILD=debug
BUILD ?= debug

# Build-specific flags and directory
ifeq ($(BUILD), debug)
    # Debug Build Flags
    CXXFLAGS := -g -O0 -D_DEBUG
    BUILD_SUBDIR := debug
else
    # Release Build Flags
    CXXFLAGS := -O3 -DNDEBUG
    BUILD_SUBDIR := release
endif

# Full path for the build directory for this configuration
# This will resolve to ../build/debug or ../build/release
CONFIG_BUILD_DIR := $(BUILD_DIR)/$(BUILD_SUBDIR)

# Compiler and Archiver
CXX := g++
AR := ar
ARFLAGS := rcs

# --- File Paths ---

# Source files
IMPL_SRCS := $(wildcard $(IMPL_DIR)/*.cpp)

# Object files for the static library
# Note: The .o files are placed in the CONFIG_BUILD_DIR
LIB_OBJS := $(patsubst $(IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/%.o, $(IMPL_SRCS))

# Path for the static library and test executable
STATIC_LIB := $(CONFIG_BUILD_DIR)/$(LIB_NAME)
TEST_BIN := $(CONFIG_BUILD_DIR)/$(TEST_EXEC)

# --- Targets ---

.PHONY: all btree btree_test clean

# Target to generate the static library
btree: $(STATIC_LIB)

# Target to compile the test executable and link the static library
btree_test: $(TEST_BIN)

# Rule to create the configuration-specific build directory
$(CONFIG_BUILD_DIR):
	@mkdir -p $@
	@echo "Using build directory: $(CONFIG_BUILD_DIR) for $(BUILD) build."

# Rule to create the static library (.a)
# It depends on all object files and the build directory existing
$(STATIC_LIB): $(LIB_OBJS) | $(CONFIG_BUILD_DIR)
	@echo "Archiving static library: $@"
	$(AR) $(ARFLAGS) $@ $^

# Rule to compile the implementation files (.cpp) into object files (.o)
# The object files are placed inside the config-specific build directory
$(CONFIG_BUILD_DIR)/%.o: $(IMPL_DIR)/%.cpp | $(CONFIG_BUILD_DIR)
	@echo "Compiling $< to $@"
	$(CXX) $(CXXFLAGS) -c $< -o $@ -I$(HEADERS_DIR)

# Rule to compile the test file and link with the static library
$(TEST_BIN): $(TEST_FILE) $(STATIC_LIB) | $(CONFIG_BUILD_DIR)
	@echo "Linking test executable: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ -I$(HEADERS_DIR)

# --- Utility Targets ---

clean:
	@echo "Cleaning up build directories..."
	@rm -rf $(BUILD_DIR)
