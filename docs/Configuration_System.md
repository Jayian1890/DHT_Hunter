# DHT Hunter Configuration System

This document provides detailed information about the DHT Hunter configuration system, including all available settings, their default values, and how to customize them.

## Table of Contents
- [Overview](#overview)
- [Configuration File Location](#configuration-file-location)
- [Command Line Arguments](#command-line-arguments)
- [Configuration Categories](#configuration-categories)
  - [General Settings](#general-settings)
  - [DHT Settings](#dht-settings)
  - [Network Settings](#network-settings)
  - [Web Interface Settings](#web-interface-settings)
  - [Persistence Settings](#persistence-settings)
  - [Crawler Settings](#crawler-settings)
  - [Metadata Acquisition Settings](#metadata-acquisition-settings)
  - [Event System Settings](#event-system-settings)
  - [Logging Settings](#logging-settings)
- [Examples](#examples)

## Overview

DHT Hunter uses a JSON configuration file to store all application settings. This allows for easy customization of the application's behavior without recompiling the code. The configuration file is loaded at application startup and can be specified via command-line arguments.

## Configuration File Location

By default, the configuration file is located at:
- Windows: `%USERPROFILE%\dht-hunter\dht_hunter.json`
- Unix/Linux/macOS: `~/dht-hunter/dht_hunter.json`

You can specify a different location using the `--config-file` command-line argument.

## Command Line Arguments

The following command-line arguments are available for configuring the application:

| Argument | Short | Description |
|----------|-------|-------------|
| `--config-dir` | `-c` | Specifies the configuration directory |
| `--config-file` | `-f` | Specifies the configuration file path |
| `--generate-config` | `-g` | Generates a default configuration file |
| `--web-root` | `-w` | Specifies the web root directory |
| `--web-port` | `-p` | Specifies the web server port |

Example:
```
./dht_hunter --config-file /path/to/config.json --web-port 8081
```

To generate a default configuration file:
```
./dht_hunter --generate-config
```

## Configuration Categories

The configuration file is organized into several categories, each containing related settings.

### General Settings

These settings control general application behavior.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `general.configDir` | string | `~/dht-hunter` | Directory for storing configuration and data files |
| `general.logFile` | string | `dht_hunter.log` | Log file name |
| `general.logLevel` | string | `info` | Log level (trace, debug, info, warning, error, critical) |

### DHT Settings

These settings control the DHT node behavior.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `dht.port` | integer | `6881` | UDP port for DHT communication |
| `dht.kBucketSize` | integer | `16` | K-bucket size (maximum number of nodes in a bucket) |
| `dht.alpha` | integer | `3` | Alpha parameter for parallel lookups (number of concurrent requests) |
| `dht.maxResults` | integer | `8` | Maximum number of nodes to store in a lookup result |
| `dht.tokenRotationInterval` | integer | `300` | Token rotation interval in seconds (5 minutes as per BEP-5) |
| `dht.bucketRefreshInterval` | integer | `60` | Bucket refresh interval in minutes |
| `dht.maxIterations` | integer | `10` | Maximum number of iterations for node lookups |
| `dht.maxQueries` | integer | `100` | Maximum number of queries for node lookups |
| `dht.bootstrapNodes` | array | See below | Bootstrap nodes for initial DHT connection |

Default bootstrap nodes:
```json
[
  "dht.aelitis.com:6881",
  "dht.transmissionbt.com:6881",
  "dht.libtorrent.org:25401",
  "router.utorrent.com:6881"
]
```

### Network Settings

These settings control network-related behavior.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `network.transactionTimeout` | integer | `30` | Transaction timeout in seconds |
| `network.maxTransactions` | integer | `1024` | Maximum number of concurrent transactions |
| `network.mtuSize` | integer | `1400` | MTU size for UDP packets |

### Web Interface Settings

These settings control the web interface.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `web.port` | integer | `8080` | Web server port |
| `web.webRoot` | string | `web` | Web root directory |

### Persistence Settings

These settings control data persistence.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `persistence.saveInterval` | integer | `60` | Save interval in minutes |
| `persistence.routingTablePath` | string | `routing_table.dat` | Routing table file path |
| `persistence.peerStoragePath` | string | `peer_storage.dat` | Peer storage file path |
| `persistence.metadataPath` | string | `metadata.dat` | Metadata file path |
| `persistence.nodeIDPath` | string | `node_id.dat` | Node ID file path |

### Crawler Settings

These settings control the DHT crawler.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `crawler.parallelCrawls` | integer | `10` | Number of nodes to crawl in parallel |
| `crawler.refreshInterval` | integer | `15` | Refresh interval in seconds |
| `crawler.maxNodes` | integer | `1000000` | Maximum number of nodes to store |
| `crawler.maxInfoHashes` | integer | `1000000` | Maximum number of info hashes to track |
| `crawler.autoStart` | boolean | `true` | Whether to automatically start crawling on initialization |

### Metadata Acquisition Settings

These settings control metadata acquisition.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `metadata.processingInterval` | integer | `5` | Processing interval in seconds |
| `metadata.maxConcurrentAcquisitions` | integer | `5` | Maximum number of concurrent metadata acquisitions |
| `metadata.acquisitionTimeout` | integer | `60` | Acquisition timeout in seconds |
| `metadata.maxRetryCount` | integer | `3` | Maximum number of retry attempts |
| `metadata.retryDelayBase` | integer | `300` | Base delay for retry in seconds (exponential backoff) |

### Event System Settings

These settings control the unified event system.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `event.enableLogging` | boolean | `true` | Whether to enable logging |
| `event.enableComponent` | boolean | `true` | Whether to enable component communication |
| `event.enableStatistics` | boolean | `true` | Whether to enable statistics |
| `event.asyncProcessing` | boolean | `false` | Whether to process events asynchronously |
| `event.eventQueueSize` | integer | `1000` | Maximum size of the event queue |
| `event.processingThreads` | integer | `1` | Number of processing threads for asynchronous processing |

### Logging Settings

These settings control logging behavior.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `logging.consoleOutput` | boolean | `true` | Whether to output logs to the console |
| `logging.fileOutput` | boolean | `true` | Whether to output logs to a file |
| `logging.includeTimestamp` | boolean | `true` | Whether to include timestamps in log messages |
| `logging.includeSeverity` | boolean | `true` | Whether to include severity levels in log messages |
| `logging.includeSource` | boolean | `true` | Whether to include source components in log messages |

## Examples

### Minimal Configuration

```json
{
  "general": {
    "logLevel": "info"
  },
  "dht": {
    "port": 6881
  },
  "web": {
    "port": 8080
  }
}
```

### Full Configuration

```json
{
  "general": {
    "configDir": "~/dht-hunter",
    "logFile": "dht_hunter.log",
    "logLevel": "info"
  },
  "dht": {
    "port": 6881,
    "kBucketSize": 16,
    "alpha": 3,
    "maxResults": 8,
    "tokenRotationInterval": 300,
    "bucketRefreshInterval": 60,
    "maxIterations": 10,
    "maxQueries": 100,
    "bootstrapNodes": [
      "dht.aelitis.com:6881",
      "dht.transmissionbt.com:6881",
      "dht.libtorrent.org:25401",
      "router.utorrent.com:6881"
    ]
  },
  "network": {
    "transactionTimeout": 30,
    "maxTransactions": 1024,
    "mtuSize": 1400
  },
  "web": {
    "port": 8080,
    "webRoot": "web"
  },
  "persistence": {
    "saveInterval": 60,
    "routingTablePath": "routing_table.dat",
    "peerStoragePath": "peer_storage.dat",
    "metadataPath": "metadata.dat",
    "nodeIDPath": "node_id.dat"
  },
  "crawler": {
    "parallelCrawls": 10,
    "refreshInterval": 15,
    "maxNodes": 1000000,
    "maxInfoHashes": 1000000,
    "autoStart": true
  },
  "metadata": {
    "processingInterval": 5,
    "maxConcurrentAcquisitions": 5,
    "acquisitionTimeout": 60,
    "maxRetryCount": 3,
    "retryDelayBase": 300
  },
  "event": {
    "enableLogging": true,
    "enableComponent": true,
    "enableStatistics": true,
    "asyncProcessing": false,
    "eventQueueSize": 1000,
    "processingThreads": 1
  },
  "logging": {
    "consoleOutput": true,
    "fileOutput": true,
    "includeTimestamp": true,
    "includeSeverity": true,
    "includeSource": true
  }
}
```
