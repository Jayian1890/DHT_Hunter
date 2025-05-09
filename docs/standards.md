# BitScrape Coding Standards

This document outlines the coding standards for the BitScrape project, with a focus on the optimized codebase structure.

## 1. File Organization

### 1.1 Directory Structure

The optimized directory structure follows a module-based approach:

```
include/
  ├── common.hpp                 # Precompiled header
  ├── dht/
  │   ├── dht_core.hpp           # Core DHT functionality
  │   ├── node_lookup.hpp        # Node lookup functionality
  │   ├── peer_lookup.hpp        # Peer lookup functionality
  │   ├── routing.hpp            # Routing functionality
  │   └── ...
  ├── network/
  │   ├── udp_network.hpp        # UDP networking
  │   ├── http_network.hpp       # HTTP networking
  │   ├── network_common.hpp     # Common network types
  │   └── ...
  ├── metadata/
  │   ├── metadata_services.hpp  # Metadata acquisition
  │   ├── peer_connection.hpp    # Peer connection management
  │   └── ...
  ├── event/
  │   ├── event_system.hpp       # Core event system
  │   ├── event_adapters.hpp     # Event adapters
  │   ├── event_processors.hpp   # Event processors
  │   └── ...
  └── utils/
      ├── common_utils.hpp       # Common utilities
      ├── domain_utils.hpp       # Domain-specific utilities
      └── system_utils.hpp       # System-related utilities

src/
  ├── dht/
  │   ├── dht_core.cpp
  │   ├── node_lookup.cpp
  │   └── ...
  ├── network/
  │   ├── udp_network.cpp
  │   └── ...
  └── ...
```

### 1.2 File Naming

- Use lowercase with underscores for file names: `file_name.hpp`, `file_name.cpp`
- Header files use `.hpp` extension
- Implementation files use `.cpp` extension
- Files should be named after the primary class or functionality they contain

### 1.3 File Structure

- Each header file should have a corresponding implementation file (except for template-only headers)
- Header files should contain minimal includes, using forward declarations where possible
- Implementation files should include their corresponding header file first

## 2. Naming Conventions

### 2.1 Classes and Types

- Use PascalCase for class names: `class DhtNode`
- Use PascalCase for type aliases: `using NodeList = std::vector<Node>`
- Use PascalCase for enum names and enum values: `enum class MessageType { Query, Response, Error }`

### 2.2 Variables

- Use camelCase for variable names: `nodeId`, `peerList`
- Use m_camelCase for member variables: `m_nodeId`, `m_peerList`
- Use s_camelCase for static variables: `s_instanceCount`
- Use g_camelCase for global variables (avoid if possible): `g_logger`

### 2.3 Functions and Methods

- Use camelCase for function and method names: `findNode()`, `getPeers()`
- Use descriptive names that indicate the action being performed

### 2.4 Constants

- Use UPPER_CASE for constants and macros: `MAX_NODES`, `DEFAULT_PORT`
- Use constexpr instead of #define for constants where possible

### 2.5 Namespaces

- Use lowercase for namespace names: `namespace dht_hunter`
- Use nested namespaces for logical grouping: `namespace dht_hunter::dht`

## 3. Code Style

### 3.1 Indentation and Formatting

- Use 4 spaces for indentation (no tabs)
- Place opening braces on the same line for functions and control structures
- Place opening braces on a new line for class and namespace definitions
- Use spaces around operators: `a + b`, not `a+b`
- Use spaces after commas: `foo(a, b, c)`, not `foo(a,b,c)`

### 3.2 Line Length and Wrapping

- Aim for a maximum line length of 100 characters
- When wrapping function declarations or calls, align parameters:
  ```cpp
  void longFunctionName(int parameter1,
                        int parameter2,
                        int parameter3);
  ```

### 3.3 Comments

- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Use Doxygen-style comments for public API documentation:
  ```cpp
  /**
   * @brief Brief description
   *
   * Detailed description
   *
   * @param param1 Description of param1
   * @return Description of return value
   */
  ```

## 4. Consolidated File Guidelines

### 4.1 Header Organization

Consolidated header files should follow this organization:

```cpp
#pragma once

// Include guard comment
// File description

// Standard library includes
#include <vector>
#include <string>

// Project includes
#include "common.hpp"

// Forward declarations
namespace dht_hunter {
    class SomeClass;
}

namespace dht_hunter {
namespace module_name {

// Constants and type aliases

// Class declarations

// Free functions

} // namespace module_name
} // namespace dht_hunter
```

### 4.2 Implementation Organization

Consolidated implementation files should follow this organization:

```cpp
// File description

#include "module_name/file_name.hpp"

// Additional includes

namespace dht_hunter {
namespace module_name {

// Class implementations

// Free function implementations

} // namespace module_name
} // namespace dht_hunter
```

### 4.3 Namespace Usage

- Use namespace blocks instead of individual using declarations
- Avoid `using namespace` in header files
- Group related functionality in logical namespaces

## 5. Best Practices

### 5.1 Modern C++ Features

- Use auto for complex types or when the type is obvious
- Use range-based for loops when possible
- Use nullptr instead of NULL or 0
- Use constexpr for compile-time constants
- Use std::unique_ptr for exclusive ownership
- Use std::shared_ptr only when shared ownership is necessary
- Use std::optional for optional values
- Use std::variant for type-safe unions

### 5.2 Error Handling

- Use exceptions for exceptional conditions
- Use std::optional or Result<T> for expected failures
- Document error conditions and handling

### 5.3 Memory Management

- Prefer stack allocation over heap allocation
- Use smart pointers instead of raw pointers for ownership
- Use references instead of pointers for non-ownership scenarios
- Avoid manual memory management with new/delete

### 5.4 Performance Considerations

- Use move semantics for large objects
- Use const references for function parameters
- Use inline for small, frequently called functions
- Consider pass-by-value for small, copyable types

## 6. Documentation

- Document all public APIs
- Document complex algorithms and non-obvious code
- Keep documentation up-to-date with code changes
- Use consistent documentation style
