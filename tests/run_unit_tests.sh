#!/bin/bash
# Unit test runner for BitScrape
# This script runs all unit tests in the unit directory

# Set the path to the unit tests directory
UNIT_DIR="$(dirname "$0")/unit"

# Check if the unit tests directory exists
if [ ! -d "$UNIT_DIR" ]; then
    echo "Error: Unit tests directory not found at $UNIT_DIR"
    exit 1
fi

# Set the path to the build directory
BUILD_DIR="/Volumes/Media/projects/coding/bitscrape/build"

# Check if the build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found at $BUILD_DIR"
    echo "Please build the project first"
    exit 1
fi

# Find all test executables in the build directory
TEST_EXECUTABLES=$(find "$BUILD_DIR" -name "test_*" -type f -executable)

# Check if any test executables were found
if [ -z "$TEST_EXECUTABLES" ]; then
    echo "Error: No test executables found in $BUILD_DIR"
    echo "Please build the project with tests enabled"
    exit 1
fi

# Initialize counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Run each test executable
for TEST_EXECUTABLE in $TEST_EXECUTABLES; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    TEST_NAME=$(basename "$TEST_EXECUTABLE")
    echo "Running test: $TEST_NAME"
    
    # Run the test executable
    "$TEST_EXECUTABLE"
    EXIT_CODE=$?
    
    # Check the exit code
    if [ $EXIT_CODE -eq 0 ]; then
        echo "  ✅ Test passed: $TEST_NAME"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "  ❌ Test failed: $TEST_NAME (exit code: $EXIT_CODE)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    echo ""
done

# Print summary
echo "Test Summary:"
echo "  Total tests: $TOTAL_TESTS"
echo "  Passed tests: $PASSED_TESTS"
echo "  Failed tests: $FAILED_TESTS"

# Exit with non-zero code if any tests failed
if [ $FAILED_TESTS -gt 0 ]; then
    exit 1
fi

exit 0
