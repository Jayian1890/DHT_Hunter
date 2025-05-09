#!/bin/bash
# Simple test runner for BitScrape unit tests

# Check if a test file was provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <test_file.cpp>"
    exit 1
fi

TEST_FILE="$1"
TEST_NAME=$(basename "$TEST_FILE" .cpp)
OUTPUT_DIR="build/tests"

# Create the output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Compile the test
echo "Compiling $TEST_NAME..."
g++ -std=c++23 -I include -o "$OUTPUT_DIR/$TEST_NAME" "$TEST_FILE" -L/usr/local/lib -L/opt/homebrew/lib -lcrypto -lssl -pthread

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Run the test
echo "Running $TEST_NAME..."
"$OUTPUT_DIR/$TEST_NAME"

# Check the exit code
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    echo "Test passed!"
else
    echo "Test failed with exit code $EXIT_CODE"
fi

exit $EXIT_CODE
