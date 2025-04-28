# Utility Namespace Migration

This document describes the migration from the old `util` namespace to the new `utility` namespace in the DHT-Hunter project.

## Overview

The utility functions in the DHT-Hunter project have been reorganized to follow a more modular and organized structure. The old `util` namespace has been renamed to `utility` and all utility functions have been organized into appropriate sub-namespaces.

## Changes Made

1. **Renamed Directory**: The `util` directory has been renamed to `utility` to better reflect its purpose.

2. **Created Sub-namespaces**: Utility functions have been organized into appropriate sub-namespaces:
   - `utility::string`: String manipulation utilities
   - `utility::random`: Random generation utilities
   - `utility::node`: Node-related utilities
   - `utility::hash`: Hash computation utilities
   - `utility::filesystem`: Filesystem utilities
   - `utility::thread`: Thread and synchronization utilities
   - `utility::process`: Process-related utilities

3. **Updated References**: All references to the old `util` namespace have been updated to use the new `utility` namespace structure.

4. **Removed Old Files**: All old utility files have been removed to avoid confusion.

5. **Updated Build System**: The build system has been updated to use the new utility directory structure.

## Migration Steps

The migration involved the following steps:

1. **Create New Directory Structure**: Created a new directory structure for the utility namespace with appropriate sub-namespaces.

2. **Move Files**: Moved utility files to their appropriate sub-directories.

3. **Update Namespace**: Updated the namespace in all utility files to use the new namespace structure.

4. **Update References**: Updated all references to the old namespace in the codebase to use the new namespace structure.

5. **Remove Old Files**: Removed all old utility files to avoid confusion.

## Specific Changes

### String Utilities

- Moved string manipulation functions to the `utility::string` namespace
- Updated all references to use the new namespace

### Random Utilities

- Moved random generation functions to the `utility::random` namespace
- Updated all references to use the new namespace

### Node Utilities

- Moved node-related functions to the `utility::node` namespace
- Updated all references to use the new namespace

### Hash Utilities

- Moved hash computation functions to the `utility::hash` namespace
- Updated all references to use the new namespace

### Filesystem Utilities

- Moved filesystem functions to the `utility::filesystem` namespace
- Updated all references to use the new namespace

### Thread Utilities

- Moved thread and synchronization functions to the `utility::thread` namespace
- Updated all references to use the new namespace
- Updated the thread pool implementation to use std::invoke_result instead of std::result_of (which is deprecated in C++20)

### Process Utilities

- Moved process-related functions to the `utility::process` namespace
- Updated all references to use the new namespace

## Benefits

The reorganization provides several benefits:

1. **Better Organization**: Utility functions are now organized by category, making it easier to find and use them.

2. **Clearer Namespace Structure**: The namespace structure is now more intuitive and follows modern C++ practices.

3. **More Maintainable**: Changes to utility functions are now isolated to their respective namespaces, making the code more maintainable.

4. **Better Separation of Concerns**: Each utility category has its own namespace, providing better separation of concerns.

## Usage

To use the utility functions, include the appropriate header file and use the fully qualified namespace:

```cpp
#include "dht_hunter/utility/string/string_utils.hpp"
#include "dht_hunter/utility/random/random_utils.hpp"

// Use the utility functions
std::string hex = dht_hunter::utility::string::bytesToHex(data, length);
std::string token = dht_hunter::utility::random::generateToken();
```

Alternatively, you can use a namespace alias:

```cpp
#include "dht_hunter/utility/string/string_utils.hpp"
namespace str_util = dht_hunter::utility::string;

// Use the utility functions
std::string hex = str_util::bytesToHex(data, length);
```

For convenience, you can include the main utility header to get access to all utility functions:

```cpp
#include "dht_hunter/utility/utility.hpp"

// Use the utility functions
std::string hex = dht_hunter::utility::string::bytesToHex(data, length);
std::string token = dht_hunter::utility::random::generateToken();
```
