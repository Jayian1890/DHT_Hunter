# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Support for metadata acquisition through the DHT
- New `findNodesWithMetadata` method in the DHTNode class

### Changed
- Refactored DHT core components to improve code organization
- Moved DHT core functionality from `dht/core/` to `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`
- Simplified the DHTNode class to provide a cleaner interface
- Made the DHTNode constructor and key methods public
- Updated the ApplicationController to work with the new DHTNode and DHTConfig classes

### Fixed
- Fixed build issues related to missing DHT constants
- Fixed metadata acquisition in the DHTMetadataProvider

## [1.0.0] - 2025-05-01

### Added
- Initial release of BitScrape
- DHT crawling and monitoring
- BitTorrent metadata acquisition
- Web interface for monitoring and data access
- Support for multiple DHT implementations (Mainline, Kademlia, Azureus)
- Configuration system with JSON support
- Logging system with multiple backends
- Event system for component communication
