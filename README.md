# BitScrape

A BitTorrent DHT crawler and metadata scraper.

## Building

BitScrape uses Meson as its build system. The following instructions will help you build the project.

### Prerequisites

- C++23 compatible compiler (GCC 11+, Clang 14+, or MSVC 19.29+)
- Meson build system
- Ninja build tool
- Python 3.6+
- fmt library
- libcurl
- pkg-config (for dependency resolution)

#### Installing Prerequisites on macOS

```bash
brew install meson ninja python3 pkg-config fmt curl
```

#### Installing Prerequisites on Ubuntu/Debian

```bash
sudo apt install meson ninja-build python3 libfmt-dev libcurl4-openssl-dev pkg-config
```

#### Installing Prerequisites on Windows

```bash
# Using vcpkg
vcpkg install fmt curl

# Using Chocolatey
choco install meson ninja python
```

### Building with Meson

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



## Running

After building, you can run the application:

```bash
# Run the CLI version
./build/bitscrape_cli

# Run the GUI version (macOS)
./build/BitScrape.app/Contents/MacOS/BitScrape
```

## Development

### VS Code Integration

The project includes VS Code configuration files for Meson. To use Meson in VS Code:

1. Install the recommended extensions (Meson, C/C++, LLDB)
2. Open the project in VS Code
3. Use the "Meson: build" task to build the project
4. Use the "Debug CLI" or "Debug App" configurations to debug

## License

[MIT License](LICENSE)
