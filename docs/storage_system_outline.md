# DHT Hunter Storage System: Phased Implementation Plan

## Overview

A pragmatic, phased approach to implementing a scalable storage system for DHT Hunter's torrent metadata. This plan focuses on delivering incremental value with each phase while building toward a comprehensive solution.

## Implementation Progress Tracking

| Phase | Description | Status | Completion % | Notes |
|-------|-------------|--------|--------------|-------|
| 1     | Basic File-Based Storage | Not Started | 0% | |
| 2     | Memory-Optimized Storage | Not Started | 0% | |
| 3     | Indexed Storage | Not Started | 0% | |
| 4     | Advanced Features | Not Started | 0% | |

## Core Requirements

- Store and retrieve millions of torrent metadata entries efficiently
- Provide fast lookup by info hash
- Support basic filtering and searching
- Ensure data durability and integrity
- Minimize memory usage

## Phase 1: Basic File-Based Storage (1-2 weeks)

### Goals
- Implement a simple, reliable storage system for torrent metadata
- Replace the current in-memory storage with persistent storage

### Implementation Checklist

| Task | Status | Completion % | Notes |
|------|--------|--------------|-------|
| **File Organization** | Not Started | 0% | |
| - Directory structure with sharding | Not Started | 0% | |
| - File naming convention | Not Started | 0% | |
| **Core Operations** | Not Started | 0% | |
| - `bool addMetadata(infoHash, data, size)` | Not Started | 0% | |
| - `std::pair<data, size> getMetadata(infoHash)` | Not Started | 0% | |
| - `bool removeMetadata(infoHash)` | Not Started | 0% | |
| - `bool exists(infoHash)` | Not Started | 0% | |
| **Index File** | Not Started | 0% | |
| - Info hash to file mapping | Not Started | 0% | |
| - Periodic flushing mechanism | Not Started | 0% | |
| **Integration** | Not Started | 0% | |
| - Update `MetadataStorage` class | Not Started | 0% | |
| **Testing** | Not Started | 0% | |
| - Unit tests | Not Started | 0% | |
| - Performance tests | Not Started | 0% | |

### Implementation Details
1. **File Organization**:
   - Store each metadata blob in a separate file named by its info hash
   - Use a directory structure with sharding (e.g., first 2 hex chars as subdirectories)

2. **Core Operations**:
   - `bool addMetadata(infoHash, data, size)`
   - `std::pair<data, size> getMetadata(infoHash)`
   - `bool removeMetadata(infoHash)`
   - `bool exists(infoHash)`

3. **Index File**:
   - Simple index file mapping info hashes to metadata files
   - Periodically flush to disk for durability

4. **Integration**:
   - Update the existing `MetadataStorage` class to use the new storage backend

## Phase 2: Memory-Optimized Storage (2-3 weeks)

### Goals
- Reduce memory usage while maintaining performance
- Support efficient batch operations
- Add basic metadata extraction for searching

### Implementation Checklist

| Task | Status | Completion % | Notes |
|------|--------|--------------|-------|
| **LRU Cache** | Not Started | 0% | |
| - Cache implementation | Not Started | 0% | |
| - Memory usage configuration | Not Started | 0% | |
| - Eviction policy | Not Started | 0% | |
| **Metadata Extraction** | Not Started | 0% | |
| - Bencode parser | Not Started | 0% | |
| - Metadata structure | Not Started | 0% | |
| - Index file format | Not Started | 0% | |
| **Batch Operations** | Not Started | 0% | |
| - Batch add implementation | Not Started | 0% | |
| - Batch get implementation | Not Started | 0% | |
| **Background Processing** | Not Started | 0% | |
| - Thread pool | Not Started | 0% | |
| - Task queue | Not Started | 0% | |
| **Testing** | Not Started | 0% | |
| - Unit tests | Not Started | 0% | |
| - Performance tests | Not Started | 0% | |

### Implementation Details
1. **LRU Cache**:
   - Implement an LRU cache for frequently accessed metadata
   - Configurable maximum memory usage
   - Automatic eviction when memory limit is reached

2. **Metadata Extraction**:
   - Extract basic metadata (name, size, file count, file types)
   - Store extracted metadata in a separate index file

3. **Batch Operations**:
   - `bool addMetadataBatch(vector<metadata>)`
   - `vector<metadata> getMetadataBatch(vector<infoHash>)`

4. **Background Processing**:
   - Process metadata extraction in background threads
   - Implement a task queue for background operations

## Phase 3: Indexed Storage (3-4 weeks)

### Goals
- Enable efficient searching and filtering
- Support pagination and sorting
- Improve overall performance

### Implementation Checklist

| Task | Status | Completion % | Notes |
|------|--------|--------------|-------|
| **Primary Index** | Not Started | 0% | |
| - B-tree implementation | Not Started | 0% | |
| - Memory mapping | Not Started | 0% | |
| **Secondary Indices** | Not Started | 0% | |
| - Name index | Not Started | 0% | |
| - Size index | Not Started | 0% | |
| - Date index | Not Started | 0% | |
| - File type index | Not Started | 0% | |
| **Query API** | Not Started | 0% | |
| - Name search | Not Started | 0% | |
| - Size filtering | Not Started | 0% | |
| - File type filtering | Not Started | 0% | |
| **Index Maintenance** | Not Started | 0% | |
| - Reindexing mechanism | Not Started | 0% | |
| - Background updates | Not Started | 0% | |
| **Testing** | Not Started | 0% | |
| - Unit tests | Not Started | 0% | |
| - Performance tests | Not Started | 0% | |

### Implementation Details
1. **Primary Index**:
   - B-tree index for fast info hash lookups
   - Memory-mapped for performance

2. **Secondary Indices**:
   - Name index (for text search)
   - Size index (for filtering by size)
   - Date index (for sorting by date)
   - File type index (for filtering by file type)

3. **Query API**:
   - `vector<metadata> findByName(string name, int limit, int offset)`
   - `vector<metadata> findBySize(uint64_t minSize, uint64_t maxSize, int limit, int offset)`
   - `vector<metadata> findByFileType(string fileType, int limit, int offset)`

4. **Index Maintenance**:
   - Periodic reindexing for optimal performance
   - Background index updates

## Phase 4: Advanced Features (4+ weeks)

### Goals
- Add advanced search capabilities
- Implement data analytics
- Support backup and recovery

### Implementation Checklist

| Task | Status | Completion % | Notes |
|------|--------|--------------|-------|
| **Full-Text Search** | Not Started | 0% | |
| - Inverted index | Not Started | 0% | |
| - Phrase search | Not Started | 0% | |
| - Fuzzy matching | Not Started | 0% | |
| **Data Analytics** | Not Started | 0% | |
| - Content type tracking | Not Started | 0% | |
| - Statistics generation | Not Started | 0% | |
| - Duplicate detection | Not Started | 0% | |
| **Backup and Recovery** | Not Started | 0% | |
| - Incremental backups | Not Started | 0% | |
| - Point-in-time recovery | Not Started | 0% | |
| - Data integrity checks | Not Started | 0% | |
| **Performance Optimization** | Not Started | 0% | |
| - Compression | Not Started | 0% | |
| - Bloom filters | Not Started | 0% | |
| - I/O optimization | Not Started | 0% | |
| **Testing** | Not Started | 0% | |
| - Unit tests | Not Started | 0% | |
| - Performance tests | Not Started | 0% | |

### Implementation Details
1. **Full-Text Search**:
   - Implement inverted index for torrent names and file names
   - Support phrase searches and fuzzy matching

2. **Data Analytics**:
   - Track popular content types
   - Generate statistics on torrent sizes, file types, etc.
   - Identify duplicate content

3. **Backup and Recovery**:
   - Implement incremental backups
   - Support point-in-time recovery
   - Add data integrity checks

4. **Performance Optimization**:
   - Implement compression for metadata storage
   - Add bloom filters for faster negative lookups
   - Optimize disk I/O patterns

## Data Structures

### Metadata Entry
```cpp
struct MetadataEntry {
    std::array<uint8_t, 20> infoHash;     // Primary key
    uint32_t size;                         // Size of the metadata
    std::vector<uint8_t> rawMetadata;      // Raw bencode metadata
    uint64_t creationTime;                 // When this entry was created
    uint64_t lastAccessTime;               // When this entry was last accessed
};
```

### Extracted Metadata
```cpp
struct ExtractedMetadata {
    std::array<uint8_t, 20> infoHash;     // Reference to the metadata
    std::string name;                      // Torrent name
    uint64_t totalSize;                    // Total size of the torrent
    uint32_t fileCount;                    // Number of files
    std::vector<std::string> fileTypes;    // Types of files in the torrent
    uint64_t creationDate;                 // Creation date from the torrent
};
```

## Integration with DHT Hunter

1. **Update MetadataStorage Class**:
   - Modify the existing class to use the new storage backend

2. **Add New Query Methods**:
   - Extend the API with new search and filtering capabilities
   - Implement pagination for large result sets

3. **Update UI** (if applicable):
   - Add search and filter controls
   - Implement result pagination
   - Display additional metadata

## Success Metrics

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| **Storage Efficiency** | 10+ million entries | - | Not Tested |
| **Memory Usage** | <1GB RAM | - | Not Tested |
| **Lookup Performance** | <10ms | - | Not Tested |
| **Search Performance** | <100ms | - | Not Tested |
| **Durability** | No data loss on crashes | - | Not Tested |
