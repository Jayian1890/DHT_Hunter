# Product Context: BitScrape

## Why This Project Exists

BitScrape exists to provide researchers, developers, and BitTorrent enthusiasts with a tool to explore and analyze the BitTorrent Distributed Hash Table (DHT) network. The BitTorrent DHT is a vast, decentralized network containing millions of nodes and infohashes, but exploring this network requires specialized tools that can efficiently crawl the DHT, discover content, and extract metadata.

## Problems BitScrape Solves

1. **DHT Network Exploration**: The BitTorrent DHT network is difficult to explore without specialized tools. BitScrape provides a robust crawler that can efficiently discover nodes and infohashes in the network.

2. **Metadata Acquisition**: While the DHT network contains infohashes (unique identifiers for torrents), it doesn't provide the associated metadata (file names, sizes, etc.). BitScrape solves this by implementing the BitTorrent metadata exchange protocol to download and parse this information.

3. **Performance Challenges**: Crawling the DHT network and downloading metadata requires handling thousands of concurrent connections and messages. BitScrape addresses this through an optimized, event-driven architecture that can handle high throughput with minimal resource usage.

4. **Data Analysis**: Understanding the BitTorrent ecosystem requires collecting and analyzing large amounts of data. BitScrape provides storage and visualization tools to help users make sense of the collected information.

5. **Development Complexity**: Building DHT and BitTorrent protocol implementations from scratch is complex. BitScrape provides a modular architecture where components can be reused in other projects.

## How BitScrape Should Work

### User Experience

1. **Simple Setup**: Users should be able to install and configure BitScrape with minimal effort, using either pre-built binaries or building from source with clear instructions.

2. **Multiple Interfaces**: BitScrape should provide both a command-line interface for scripting and automation, and a graphical interface for interactive use.

3. **Real-time Monitoring**: Users should be able to monitor the crawler's status in real-time, including statistics on discovered nodes, infohashes, and downloaded metadata.

4. **Data Access**: Users should have easy access to the collected data, with options to export or query the database.

5. **Configuration Options**: Users should be able to customize the crawler's behavior through configuration options, such as crawling speed, target areas of the DHT, and resource limits.

### Technical Operation

1. **Bootstrap**: The crawler should bootstrap into the DHT network using well-known nodes or previously discovered nodes.

2. **Node Discovery**: The crawler should continuously discover new nodes in the DHT network through queries and responses.

3. **Infohash Discovery**: The crawler should discover infohashes through get_peers queries and store them for metadata acquisition.

4. **Metadata Download**: For discovered infohashes, the crawler should connect to peers and download the associated metadata using the BitTorrent metadata exchange protocol.

5. **Data Storage**: All discovered information should be stored in a persistent database for later analysis.

6. **Resource Management**: The crawler should manage its resources efficiently, limiting concurrent connections and prioritizing promising infohashes.

## User Experience Goals

1. **Simplicity**: BitScrape should be easy to use, with clear documentation and intuitive interfaces.

2. **Transparency**: Users should have visibility into what the crawler is doing and why, with detailed logs and statistics.

3. **Flexibility**: BitScrape should accommodate different use cases, from casual exploration to large-scale data collection.

4. **Performance**: The crawler should operate efficiently, making the most of available system resources without overwhelming the user's machine.

5. **Reliability**: BitScrape should be stable and reliable, with graceful handling of errors and network issues.

6. **Cross-Platform**: Users should have a consistent experience across different operating systems.
