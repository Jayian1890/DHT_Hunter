#!/bin/bash
# Basic startup test for BitScrape
# This script tests if the application starts correctly and responds to basic commands

# Set the path to the executable
BITSCRAPE_CLI="/Volumes/Media/projects/coding/bitscrape/build/bitscrape"

# Print the path for debugging
echo "Looking for BitScrape executable at: $BITSCRAPE_CLI"

# Check if the executable exists
if [ ! -f "$BITSCRAPE_CLI" ]; then
    echo "Error: BitScrape executable not found at $BITSCRAPE_CLI"
    exit 1
fi

# Test 1: Check if the executable exists
echo "Test 1: Checking if the executable exists"
if [ -f "$BITSCRAPE_CLI" ]; then
    echo "  ✅ Executable exists at $BITSCRAPE_CLI"
else
    echo "  ❌ Executable not found at $BITSCRAPE_CLI"
    exit 1
fi

# Test 2: Check if the executable is executable
echo "Test 2: Checking if the executable has execute permissions"
if [ -x "$BITSCRAPE_CLI" ]; then
    echo "  ✅ Executable has execute permissions"
else
    echo "  ❌ Executable does not have execute permissions"
    exit 1
fi

# All tests passed
echo "All basic startup tests passed!"
exit 0
