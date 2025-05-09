# Technical Context: BitScrape

## Technologies Used

### Programming Languages
- **C++23**: Primary programming language
  - Modern C++ features (auto, lambdas, ranges)
  - Standard Library for core functionality
  - STL containers and algorithms

### Build System
- **Meson**: Primary build system
  - Cross-platform build configuration
  - Dependency management
  - Build variant management (debug, release)
- **Ninja**: Build tool used by Meson
  - Fast, parallel builds
  - Incremental compilation

### Libraries
- **fmt**: Modern formatting library
  - Used for string formatting and logging
  - Replacement for printf and iostreams
- **libcurl**: HTTP client library
  - Used for HTTP requests to trackers and web services
  - SSL/TLS support for secure connections
- **SQLite**: Embedded database
  - Used for persistent storage of discovered data
  - Self-contained, zero-configuration database

### Development Tools
- **VS Code**: Primary IDE
  - C/C++ extension for IntelliSense
  - Meson extension for build integration
  - LLDB extension for debugging
- **LLDB/GDB**: Debuggers
  - Used for debugging and troubleshooting
  - Memory analysis and leak detection
- **AddressSanitizer**: Runtime error detection tool
  - Memory error detection
  - Used in debug builds

### Operating Systems
- **macOS**: Primary development platform
  - Support for .app bundle creation
  - Native UI integration
- **Linux**: Supported platform
  - Command-line interface
  - Server deployment
- **Windows**: Supported platform
  - Command-line interface
  - Native UI integration (planned)

## Development Setup

### Prerequisites
- C++23 compatible compiler (GCC 11+, Clang 14+, or MSVC 19.29+)
- Meson build system
- Ninja build tool
- Python 3.6+
- fmt library
- libcurl
- pkg-config (for dependency resolution)

### Installation Commands

#### macOS
```bash
brew install meson ninja python3 pkg-config fmt curl
```

#### Ubuntu/Debian
```bash
sudo apt install meson ninja-build python3 libfmt-dev libcurl4-openssl-dev pkg-config
```

#### Windows
```bash
# Using vcpkg
vcpkg install fmt curl

# Using Chocolatey
choco install meson ninja python
```

### Build Commands
```bash
# Configure and build
./build.sh

# Build with specific options
./build.sh --release      # Release build
./build.sh --debug --asan # Debug build with AddressSanitizer
./build.sh --no-tests     # Skip building tests
./build.sh --clean        # Clean build

# Or use Meson directly
meson setup build
meson compile -C build
```

## Technical Constraints

### Performance Constraints
- Must handle thousands of concurrent DHT connections
- Must process DHT messages with minimal latency
- Must efficiently manage memory for large routing tables
- Must handle high throughput of metadata downloads

### Compatibility Constraints
- Must work on macOS, Linux, and Windows
- Must support both 32-bit and 64-bit architectures
- Must work with various network configurations (NAT, firewalls)
- Must handle different DHT protocol variants

### Security Constraints
- Must validate all incoming network data
- Must handle malicious DHT responses
- Must protect against common network attacks
- Must not expose sensitive user information

## Dependencies

### External Dependencies
- fmt: String formatting
- libcurl: HTTP client
- SQLite: Database storage
- OpenSSL: Cryptography (via libcurl)

### Internal Dependencies
The project is organized into the following components:

1. **types**: Common type definitions
2. **utility**: Utility functions and classes
3. **network**: Network abstraction layer
4. **bencode**: Bencode encoding/decoding
5. **dht**: DHT protocol implementation
6. **bittorrent**: BitTorrent protocol implementation
7. **unified_event**: Event system
8. **web**: Web interface
9. **core**: Core application components

## Tool Usage Patterns

### Build System
- Use Meson for all build configuration
- Use build.sh script for common build tasks
- Use VS Code tasks for IDE integration

### Version Control
- Use Git for version control
- Use feature branches for development
- Use pull requests for code review

### Debugging
- Use VS Code debugging configurations
- Use AddressSanitizer for memory issues
- Use logging for runtime diagnostics

### Testing
- Use unit tests for components
- Use integration tests for system behavior
- Use manual testing for UI and user experience
