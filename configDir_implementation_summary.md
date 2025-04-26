# configDir Implementation Summary

## Overview

The `configDir` variable has been implemented across the project to provide a consistent way to handle file paths. This variable can be modified within the `main` function and is used as the preceding directory for every file/directory function/call.

## Changes Made

1. **DHTConfig Class**
   - Added a `configDir` field with default value "config"
   - Added getter/setter methods for the `configDir`
   - Added a `getFullPath` method to construct full paths by combining the `configDir` with a relative path

2. **Main Function**
   - Added command-line parameter parsing for `--config-dir` or `-c`
   - Added code to ensure the config directory exists
   - Updated the LogForge initialization to use the `configDir` for the log file
   - Updated the DHTConfig initialization to use the `configDir`

3. **RoutingManager Class**
   - Updated to use the `configDir` when loading/saving the routing table
   - Modified the `loadRoutingTable` and `saveRoutingTable` methods to use the full path

## Components Already Using configDir

Several components were already using the `configDir` correctly:

1. **DHTCrawler Class**
   - Already using the `configDir` for the DHT node configuration
   - Already using the `configDir` for the info hash collector
   - Already using the `configDir` for the metadata storage

2. **InfoHashCollector Class**
   - Already using the `configDir` for saving info hashes

3. **MetadataStorage Class**
   - Already using the `configDir` for storing metadata

## Usage

The `configDir` variable can be set via the command line:

```bash
./dht_hunter --config-dir /path/to/config
# or
./dht_hunter -c /path/to/config
```

If not specified, it defaults to "config" in the current directory.

## File Paths

All file paths in the project are now constructed using the `configDir` as the base directory:

1. **Log Files**: `{configDir}/dht_hunter.log`
2. **Routing Table**: `{configDir}/routing_table.dat`
3. **Info Hash Collection**: `{configDir}/{infoHashCollectorConfig.savePath}`
4. **Metadata Storage**: `{configDir}/{metadataStorageDirectory}`

## Benefits

1. **Consistency**: All file paths are now constructed in a consistent manner
2. **Configurability**: The config directory can be easily changed via the command line
3. **Organization**: All configuration and data files are stored in a single directory
4. **Portability**: The application can be run with different config directories for different purposes
