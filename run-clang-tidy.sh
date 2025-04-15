#!/bin/bash

# This script runs clang-tidy on all source files in the project
# Usage: ./run-clang-tidy.sh [build_dir]

# Default build directory
BUILD_DIR=${1:-build}

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist."
    echo "Please run cmake to generate the build files first, or specify a different build directory."
    exit 1
fi

# Check if compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "Error: compile_commands.json not found in '$BUILD_DIR'."
    echo "Please make sure you configured CMake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    exit 1
fi

# Find all source files
SOURCE_FILES=$(find src include -name "*.cpp" -o -name "*.hpp")

# Run clang-tidy on each file
for file in $SOURCE_FILES; do
    echo "Analyzing $file..."
    clang-tidy -p="$BUILD_DIR" "$file"
done

echo "Static analysis complete."
