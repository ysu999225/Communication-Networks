# Makefile for compiling a C++ project with multiple source files

# Compiler settings - Can change to clang++ if desired
CXX = g++
CXXFLAGS = -g -Wall -Wextra -Wno-sign-compare -std=c++11

# Define the directories for source files and object files
SRC_DIR = src
OBJ_DIR = obj

# Define the target executables
TARGETS = http_server http_client

# Automatically find source files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)

# Automatically generate object file names from sources
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGETS)

# Rule for linking the final executables
# The % pattern matches each target
$(TARGETS): %: $(OBJ_DIR)/%.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# Rule for compiling source files to object files
# Uses automatic variables:
# $<: the name of the prerequisite of the rule (a .cpp file)
# $@: the name of the target of the rule (a .o file)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target for removing compiled files
clean:
	rm -rf $(OBJ_DIR) $(TARGETS)

# Prevent make from mistaking filenames for make targets
.PHONY: all clean

