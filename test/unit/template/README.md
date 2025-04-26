# Test Template

This directory contains template files for creating new unit tests in the DHT-Hunter project.

## How to use this template

1. Copy the `template_test.cpp` and `CMakeLists.txt` files to a new directory under `test/unit/`
2. Rename the files and update the content to match your component
3. Update the `test/unit/CMakeLists.txt` file to include your new test directory

## Test Structure

Each test file should:

1. Include the necessary headers
2. Define a test fixture class that inherits from `::testing::Test`
3. Implement `SetUp()` and `TearDown()` methods if needed
4. Define test cases using the `TEST_F` macro
5. Group related tests together

## CMakeLists.txt Structure

Each test directory should have a `CMakeLists.txt` file that:

1. Defines the test executable
2. Links against the necessary libraries
3. Adds the test to CTest

## Naming Conventions

- Test files should be named `component_test.cpp`
- Test executables should be named `component_test`
- Test names in CTest should match the executable name

## Running Tests

Tests can be run using CTest:

```bash
cd build
ctest                  # Run all tests
ctest -R component     # Run tests matching 'component'
ctest -V               # Run tests with verbose output
```
