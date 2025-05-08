# Common Utilities

This directory contains common utility functions and classes that are used throughout the codebase.

## Overview

The `common_utils.hpp` file provides:

1. The `Result<T>` class for error handling
2. Common constants used throughout the codebase
3. Other common utility functions

## Usage

### Result Class

The `Result<T>` class is used for functions that can fail. It provides a way to return either a value or an error message.

```cpp
// Example of returning a successful result
Result<std::string> readFile(const std::string& filePath) {
    // ...
    return Result<std::string>(content);
}

// Example of returning a failed result
Result<std::string> readFile(const std::string& filePath) {
    // ...
    return Result<std::string>("Failed to read file: " + filePath);
}

// Example of using a result
auto result = readFile("example.txt");
if (result.isSuccess()) {
    std::string content = result.getValue();
    // ...
} else {
    std::string error = result.getError();
    // ...
}
```

### Constants

Common constants are defined in the `constants` namespace:

```cpp
using namespace dht_hunter::utility::common::constants;

// Example usage
uint16_t port = DEFAULT_DHT_PORT;
```

## Integration

This component replaces the previous `common.hpp` file in the root include directory. All functionality has been moved to the utility component for better organization.
