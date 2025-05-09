# BitScrape

BitScrape is a DHT (Distributed Hash Table) crawler and BitTorrent metadata acquisition tool.

## Features

- DHT crawling and monitoring
- BitTorrent metadata acquisition
- Web interface for monitoring and data access
- Extensible architecture for custom DHT implementations

## Building

### Prerequisites

- C++17 compatible compiler
- CMake 3.15 or higher
- OpenSSL development libraries
- Boost development libraries (optional)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/bitscrape.git
cd bitscrape

# Create a build directory
mkdir build
cd build

# Configure and build
cmake ..
make
```

### Build Options

- `BUILD_TESTS`: Build unit tests (default: ON)
- `BUILD_GUI`: Build GUI application (default: ON)
- `BUILD_CLI`: Build CLI application (default: ON)

Example:

```bash
cmake -DBUILD_TESTS=OFF -DBUILD_GUI=OFF ..
```

## Usage

### CLI Application

```bash
./bitscrape --port 6881 --web-port 8080
```

### GUI Application

```bash
./BitScrape.app/Contents/MacOS/BitScrape
```

## Configuration

BitScrape can be configured using a JSON configuration file:

```json
{
  "dht": {
    "port": 6881,
    "bootstrapNodes": [
      "router.bittorrent.com:6881",
      "dht.transmissionbt.com:6881"
    ]
  },
  "web": {
    "port": 8080,
    "enableCors": true
  }
}
```

## Recent Changes

### DHT Core Refactoring

The DHT core components were refactored to improve code organization and reduce dependencies:

- Moved DHT core functionality from `dht/core/` to `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`
- Simplified the DHTNode class to provide a cleaner interface
- Added support for metadata acquisition through the DHT

See [DHT Core Refactoring](docs/changes/dht_core_refactoring.md) for more details.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request
