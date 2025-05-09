# BitScrape Utility Dependencies

This document maps the dependencies between utility classes and identifies consolidation opportunities.

## Utility Files

### Common Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| common_utils.hpp/cpp | Core utility types and functions | Standard library |

### String Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| string_utils.hpp/cpp | String manipulation functions | Standard library |

### File Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| file_utils.hpp/cpp | File I/O operations | common_utils.hpp, filesystem |
| filesystem_utils.hpp/cpp | Filesystem operations | common_utils.hpp, filesystem |

### Hash Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| hash_utils.hpp/cpp | Hashing functions | Standard library |

### Network Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| network_utils.hpp/cpp | Network-related utilities | Standard library |

### Node Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| node_utils.hpp/cpp | DHT node-related utilities | Standard library |

### Metadata Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| metadata_utils.hpp/cpp | BitTorrent metadata utilities | Standard library |

### Thread Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| thread_utils.hpp/cpp | Thread management utilities | Standard library |
| thread_pool.hpp/cpp | Thread pool implementation | Standard library |

### Process Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| process_utils.hpp/cpp | Process management utilities | Standard library |

### System Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| memory_utils.hpp/cpp | Memory management utilities | Standard library |

### Other Utilities

| File | Purpose | Dependencies |
|------|---------|--------------|
| random_utils.hpp/cpp | Random number generation | Standard library |
| json.hpp/cpp | JSON parsing and serialization | Standard library |
| configuration_manager.hpp/cpp | Configuration management | Standard library |

## Dependency Graph

```
common_utils.hpp
  ↑
  ├── file_utils.hpp
  ├── filesystem_utils.hpp
  └── (other utilities that use Result<T>)

utility.hpp
  ↓
  ├── common_utils.hpp
  ├── string_utils.hpp
  ├── random_utils.hpp
  ├── node_utils.hpp
  ├── hash_utils.hpp
  ├── filesystem_utils.hpp
  ├── file_utils.hpp
  ├── thread_utils.hpp
  ├── thread_pool.hpp
  ├── process_utils.hpp
  └── network_utils.hpp
```

## Consolidation Opportunities

### 1. Common Utilities

The `common_utils.hpp` file already contains the `Result<T>` class, which is used by many other utilities. This file can be expanded to include more common functionality.

### 2. String and Hash Utilities

The string and hash utilities can be consolidated into a single file:

- `string_utils.hpp/cpp` and `hash_utils.hpp/cpp` → `common_utils.hpp/cpp`

These utilities are closely related and often used together (e.g., converting bytes to hex strings).

### 3. File and Filesystem Utilities

The file and filesystem utilities can be consolidated into a single file:

- `file_utils.hpp/cpp` and `filesystem_utils.hpp/cpp` → `common_utils.hpp/cpp`

These utilities are closely related and often used together.

### 4. Network and Domain Utilities

The network, node, and metadata utilities can be consolidated into a single domain-specific utility file:

- `network_utils.hpp/cpp`, `node_utils.hpp/cpp`, and `metadata_utils.hpp/cpp` → `domain_utils.hpp/cpp`

These utilities are related to the domain-specific functionality of the application.

### 5. System Utilities

The thread, process, and memory utilities can be consolidated into a single system utility file:

- `thread_pool.hpp/cpp`, `thread_utils.hpp/cpp`, `process_utils.hpp/cpp`, and `memory_utils.hpp/cpp` → `system_utils.hpp/cpp`

These utilities are related to system-level functionality.

## Consolidation Plan

### Phase 1: Common Utilities

Consolidate string, hash, file, and filesystem utilities into `common_utils.hpp/cpp`.

### Phase 2: Domain Utilities

Consolidate network, node, and metadata utilities into `domain_utils.hpp/cpp`.

### Phase 3: System Utilities

Consolidate thread, process, and memory utilities into `system_utils.hpp/cpp`.

## Namespace Structure

The consolidated utilities will use the following namespace structure:

```cpp
namespace dht_hunter::utility {
    // Common utilities
    namespace common {
        // String utilities
        namespace string {
            // String utility functions
        }
        
        // Hash utilities
        namespace hash {
            // Hash utility functions
        }
        
        // File utilities
        namespace file {
            // File utility functions
        }
        
        // Filesystem utilities
        namespace filesystem {
            // Filesystem utility functions
        }
    }
    
    // Domain utilities
    namespace domain {
        // Network utilities
        namespace network {
            // Network utility functions
        }
        
        // Node utilities
        namespace node {
            // Node utility functions
        }
        
        // Metadata utilities
        namespace metadata {
            // Metadata utility functions
        }
    }
    
    // System utilities
    namespace system {
        // Thread utilities
        namespace thread {
            // Thread utility functions
        }
        
        // Process utilities
        namespace process {
            // Process utility functions
        }
        
        // Memory utilities
        namespace memory {
            // Memory utility functions
        }
    }
}
```

This namespace structure will maintain backward compatibility while reducing the number of files.
