#!/bin/bash
# Regression test runner for BitScrape
# This script runs all regression tests in the regression directory

# Set the path to the regression tests directory
REGRESSION_DIR="$(dirname "$0")/regression"

# Check if the regression tests directory exists
if [ ! -d "$REGRESSION_DIR" ]; then
    echo "Error: Regression tests directory not found at $REGRESSION_DIR"
    exit 1
fi

# Find all test scripts in the regression directory
TEST_SCRIPTS=$(find "$REGRESSION_DIR" -name "*.sh" -type f)

# Check if any test scripts were found
if [ -z "$TEST_SCRIPTS" ]; then
    echo "Error: No test scripts found in $REGRESSION_DIR"
    exit 1
fi

# Initialize counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Run each test script
for TEST_SCRIPT in $TEST_SCRIPTS; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo "Running test: $(basename "$TEST_SCRIPT")"
    
    # Run the test script
    "$TEST_SCRIPT"
    EXIT_CODE=$?
    
    # Check the exit code
    if [ $EXIT_CODE -eq 0 ]; then
        echo "  ✅ Test passed: $(basename "$TEST_SCRIPT")"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "  ❌ Test failed: $(basename "$TEST_SCRIPT") (exit code: $EXIT_CODE)"
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
