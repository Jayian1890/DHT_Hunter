# DHT-Hunter

A cross-platform terminal-based DHT node and metadata crawler for BitTorrent networks.

## Overview

DHT-Hunter functions as a real DHT node and crawls/scrapes the DHT network for active torrent metadata. The implementation is done without relying on third-party or external libraries.

## Features

- Full DHT node implementation (Kademlia)
- BitTorrent metadata exchange
- DHT network crawler/scraper
- Cross-platform terminal UI
- Comprehensive logging system
- Efficient metadata storage

## Building

### Prerequisites

- C++20 compatible compiler
- CMake 3.14 or higher

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./dht_hunter [options]
```

## License

[AGPLv3 License](LICENSE)
