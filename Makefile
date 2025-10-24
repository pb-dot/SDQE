# How to use this makefile
# make <target> BUILD=release   ; if BUILD=  is not specified then debug build
# target can be [btree , schema , query, libs ,tests , clean]
# if target not specified build the exe from (src/main.cpp)
# btree , schema , query are for their individual static libs ,libs to make all libs
# tests for making each cpp file inside test into executable

########################################################################################
# Makefile for multi-module C++ project (located in the project root)
# Switched from static (.a) to shared (.so) libraries.

# --- 1. Global Configuration ---
SRC_DIR := src
BUILD_DIR := build
TEST_DIR := tests

# Source file for the main application
MAIN_SRC := $(SRC_DIR)/main.cpp

# Determine the build type (default to debug)
BUILD ?= debug

# Compiler and Archiver
CXX := g++
# Use CXX for linking shared libraries, not 'ar'
LD_SHARED := $(CXX) -shared

# Build-specific flags and directory (Warnings removed)
# Added -fPIC (Position Independent Code) which is required for shared libraries
ifeq ($(BUILD), debug)
    CXXFLAGS := -g -O0 -D_DEBUG -MMD -MP -fPIC
    BUILD_SUBDIR := debug
else
    CXXFLAGS := -O3 -DNDEBUG -MMD -MP -fPIC
    BUILD_SUBDIR := release
endif

# Full path for the build directory for this configuration: build/debug or build/release
CONFIG_BUILD_DIR := $(BUILD_DIR)/$(BUILD_SUBDIR)
MAIN_EXEC := $(CONFIG_BUILD_DIR)/main_app

# Add runtime path (rpath) linker flag. This tells the executables (main_app, tests)
# where to find the .so files when they run.
LDFLAGS_RUNTIME := -Wl,-rpath,$(CONFIG_BUILD_DIR)

# --- 2. Dependency-Based Includes ---

# Define local include paths for each module
BTREE_INC := -I$(SRC_DIR)/bTree/headers
SCHEMA_INC := -I$(SRC_DIR)/schemaAndData/headers
QUERY_INC := -I$(SRC_DIR)/queryExecution/headers

# Global includes for linking/tests (needs all paths)
ALL_INCLUDES := $(BTREE_INC) $(SCHEMA_INC) $(QUERY_INC)

# --- 3. Manual File Definitions ---

# --- btree (was bTree) ---
BTREE_IMPL_DIR := $(SRC_DIR)/bTree/implements
BTREE_SRCS := $(wildcard $(BTREE_IMPL_DIR)/*.cpp)
BTREE_OBJS := $(patsubst $(BTREE_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/btree/%.o, $(BTREE_SRCS))
BTREE_DEPS := $(patsubst $(BTREE_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/btree/%.d, $(BTREE_SRCS))
# Switched from .a to .so
BTREE_LIB := $(CONFIG_BUILD_DIR)/libbtree.so

# --- schema (was schemaAndData) ---
SCHEMA_IMPL_DIR := $(SRC_DIR)/schemaAndData/implements
SCHEMA_SRCS := $(wildcard $(SCHEMA_IMPL_DIR)/*.cpp)
SCHEMA_OBJS := $(patsubst $(SCHEMA_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/schema/%.o, $(SCHEMA_SRCS))
SCHEMA_DEPS := $(patsubst $(SCHEMA_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/schema/%.d, $(SCHEMA_SRCS))
# Switched from .a to .so
SCHEMA_LIB := $(CONFIG_BUILD_DIR)/libschema.so

# --- query (was queryExecution) ---
QUERY_IMPL_DIR := $(SRC_DIR)/queryExecution/implements
QUERY_SRCS := $(wildcard $(QUERY_IMPL_DIR)/*.cpp)
QUERY_OBJS := $(patsubst $(QUERY_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/query/%.o, $(QUERY_SRCS))
QUERY_DEPS := $(patsubst $(QUERY_IMPL_DIR)/%.cpp, $(CONFIG_BUILD_DIR)/query/%.d, $(QUERY_SRCS))
# Switched from .a to .so
QUERY_LIB := $(CONFIG_BUILD_DIR)/libquery.so

# --- All ---
ALL_LIBS := $(QUERY_LIB) $(SCHEMA_LIB) $(BTREE_LIB)
ALL_DEPS := $(BTREE_DEPS) $(SCHEMA_DEPS) $(QUERY_DEPS)
ALL_OBJS_DIRS := $(CONFIG_BUILD_DIR)/btree $(CONFIG_BUILD_DIR)/schema $(CONFIG_BUILD_DIR)/query

# --- 4. Test Configuration ---
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
# Executable names (e.g., btreeTest.cpp -> btreeTest)
TEST_NAMES := $(basename $(notdir $(TEST_SRCS)))
# Final binaries (e.g., build/debug/btreeTest)
TEST_BINS := $(addprefix $(CONFIG_BUILD_DIR)/, $(TEST_NAMES))


# --- 5. Targets and Rules ---

# Default Target (Builds the main executable)
.PHONY: all default clean tests libs btree schema query $(TEST_NAMES)

default: $(MAIN_EXEC)

# Main executable rule
$(MAIN_EXEC): $(MAIN_SRC) $(ALL_LIBS) | $(CONFIG_BUILD_DIR)
	@echo "Linking default executable: $@"
	$(CXX) $(CXXFLAGS) $(ALL_INCLUDES) -o $@ $< $(ALL_LIBS) $(LDFLAGS_RUNTIME)

# Target for 'make all' (Builds main executable and all tests)
all: default tests

# --- Rules for Modules (Static Libraries) ---

# NEW: Target to build all libraries
libs: $(ALL_LIBS)

# Phony targets for building each library
btree: $(BTREE_LIB)
schema: $(SCHEMA_LIB)
query: $(QUERY_LIB)

# Rule to create the configuration-specific build directory
$(CONFIG_BUILD_DIR):
	@mkdir -p $@
	@echo "Using build directory: $(CONFIG_BUILD_DIR) for $(BUILD) build."

# Rule to create the module-specific object sub-directories
$(ALL_OBJS_DIRS): $(CONFIG_BUILD_DIR)
	@mkdir -p $@

# --- Explicit Object Compilation Rules (FIXED: All use ALL_INCLUDES) ---

# Rule for btree objects
$(CONFIG_BUILD_DIR)/btree/%.o: $(SRC_DIR)/bTree/implements/%.cpp | $(ALL_OBJS_DIRS)
	@echo "Compiling [btree] $<"
	$(CXX) $(CXXFLAGS) $(ALL_INCLUDES) -c $< -o $@

# Rule for schema objects
$(CONFIG_BUILD_DIR)/schema/%.o: $(SRC_DIR)/schemaAndData/implements/%.cpp | $(ALL_OBJS_DIRS)
	@echo "Compiling [schema] $<"
	$(CXX) $(CXXFLAGS) $(ALL_INCLUDES) -c $< -o $@

# Rule for query objects
$(CONFIG_BUILD_DIR)/query/%.o: $(SRC_DIR)/queryExecution/implements/%.cpp | $(ALL_OBJS_DIRS)
	@echo "Compiling [query] $<"
	$(CXX) $(CXXFLAGS) $(ALL_INCLUDES) -c $< -o $@

# --- Explicit Library Archiving Rules (Changed from 'ar' to 'g++ -shared') ---

$(BTREE_LIB): $(BTREE_OBJS) | $(CONFIG_BUILD_DIR)
	@echo "Linking shared library: $@"
	$(LD_SHARED) -o $@ $^

$(SCHEMA_LIB): $(SCHEMA_OBJS) | $(CONFIG_BUILD_DIR)
	@echo "Linking shared library: $@"
	$(LD_SHARED) -o $@ $^

$(QUERY_LIB): $(QUERY_OBJS) | $(CONFIG_BUILD_DIR)
	@echo "Linking shared library: $@"
	$(LD_SHARED) -o $@ $^

# --- Rules for Tests (Executables) ---

# Target for 'make tests' which builds all executables
tests: $(TEST_BINS)

# Expose individual test names as phony targets (e.g., make btreeTest)
$(TEST_NAMES): $(CONFIG_BUILD_DIR)/$(@)

# Rule to build all test executables
$(CONFIG_BUILD_DIR)/%: $(TEST_DIR)/%.cpp $(ALL_LIBS) | $(CONFIG_BUILD_DIR)
	@echo "Linking test executable: $@"
	# Libraries are explicitly listed after the test source for correct linking order
	$(CXX) $(CXXFLAGS) $(ALL_INCLUDES) -o $@ $< $(ALL_LIBS) $(LDFLAGS_RUNTIME)

# --- Utility and Dependency Inclusion ---

# Include .d files for automatic dependency tracking (header changes)
-include $(ALL_DEPS)

# Clean target
clean:
	@echo "Cleaning up build directory..."
	@rm -rf $(BUILD_DIR)
