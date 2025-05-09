# Project Brief: BitScrape

## Project Overview
BitScrape is a BitTorrent DHT crawler and metadata scraper designed to efficiently discover and collect metadata from the BitTorrent distributed hash table network. The project aims to provide a high-performance, modular tool for exploring the BitTorrent ecosystem.

## Core Requirements

### Functional Requirements
1. **DHT Crawling**: Implement a robust DHT crawler that can discover nodes and infohashes in the BitTorrent network
2. **Metadata Acquisition**: Download and parse BitTorrent metadata from peers
3. **Data Storage**: Store discovered infohashes and metadata in a persistent database
4. **Monitoring Interface**: Provide a web interface for monitoring crawler status and viewing collected data
5. **Performance Optimization**: Handle high throughput of DHT messages and metadata downloads
6. **Cross-Platform Support**: Run on major operating systems (macOS, Linux, Windows)

### Technical Requirements
1. **Modular Architecture**: Implement a modular design where components can be used independently
2. **Event-Driven Design**: Use an event-driven architecture to prevent operations from deadlocking
3. **Asynchronous Operations**: Utilize asynchronous programming for network operations
4. **Resource Efficiency**: Minimize memory and CPU usage while maximizing throughput
5. **Code Quality**: Maintain high code quality with comprehensive documentation and tests
6. **Build System**: Use Meson as the primary build system

## Project Goals
1. Create a high-performance BitTorrent DHT crawler and metadata scraper
2. Implement a modular, event-driven architecture that can be extended for various use cases
3. Optimize the codebase to reduce complexity and improve maintainability
4. Provide a user-friendly interface for monitoring and data access
5. Support both CLI and GUI operation modes

## Project Scope

### In Scope
- DHT protocol implementation (Mainline, Kademlia, Azureus)
- BitTorrent metadata exchange protocol
- Persistent storage of discovered data
- Web-based monitoring interface
- CLI and GUI applications
- Cross-platform support

### Out of Scope
- Full BitTorrent client functionality (downloading/uploading content)
- Legal analysis of discovered content
- Content categorization or filtering
- Distributed crawler deployment (though the architecture should support future extension)

## Success Criteria
1. Successfully crawl the DHT network and discover active infohashes
2. Download and parse metadata for discovered infohashes
3. Store and provide access to collected data
4. Maintain stable operation with minimal resource usage
5. Provide clear documentation for users and developers

## Current Status
The project is undergoing optimization to reduce code complexity while maintaining functionality. The optimization focuses on consolidating related components, reducing redundant code, and improving the overall architecture.
