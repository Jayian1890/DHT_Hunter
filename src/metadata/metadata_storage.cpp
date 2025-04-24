#include "dht_hunter/metadata/metadata_storage.hpp"
#include "dht_hunter/metadata/torrent_file_builder.hpp"
#include "dht_hunter/util/hash.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <cerrno>

#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

DEFINE_COMPONENT_LOGGER("Metadata", "Storage")

namespace dht_hunter::metadata {

MetadataStorage::MetadataStorage(const MetadataStorageConfig& config)
    : m_config(config) {
}

MetadataStorage::~MetadataStorage() {
    shutdown();
}

bool MetadataStorage::initialize() {
    if (m_initialized) {
        getLogger()->warning("Metadata storage already initialized");
        return true;
    }

    getLogger()->debug("Initializing metadata storage with config: storageDirectory={}, persistMetadata={}, createTorrentFiles={}, torrentFilesDirectory={}",
                 m_config.storageDirectory, m_config.persistMetadata, m_config.createTorrentFiles, m_config.torrentFilesDirectory);

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create the storage directory if it doesn't exist
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path storageDir(m_config.storageDirectory);
            getLogger()->debug("Checking if storage directory exists: {}", storageDir.string());
            if (!std::filesystem::exists(storageDir)) {
                getLogger()->debug("Creating storage directory: {}", storageDir.string());
                try {
                    if (!std::filesystem::create_directories(storageDir)) {
                        getLogger()->error("Failed to create storage directory: {}", m_config.storageDirectory);
                        return false;
                    }
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while creating storage directory: {}, error: {}",
                                 m_config.storageDirectory, e.what());
                    return false;
                }
                getLogger()->debug("Created storage directory: {}", storageDir.string());
            } else {
                getLogger()->debug("Storage directory already exists: {}", storageDir.string());
            }

            // Check if the directory is writable
            try {
                // Try to create a test file
                std::string testFilePath = (storageDir / "test.tmp").string();
                std::ofstream testFile(testFilePath);
                if (!testFile) {
                    getLogger()->error("Storage directory is not writable: {}", storageDir.string());
                    return false;
                }
                testFile.close();

                // Remove the test file
                std::filesystem::remove(testFilePath);
                getLogger()->debug("Storage directory is writable: {}", storageDir.string());
            } catch (const std::exception& e) {
                getLogger()->error("Exception while testing storage directory writability: {}, error: {}",
                             storageDir.string(), e.what());
                return false;
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while creating storage directory: {}", e.what());
            return false;
        }
    }

    // Create the torrent files directory if it doesn't exist
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentDir(m_config.torrentFilesDirectory);
            getLogger()->debug("Checking if torrent files directory exists: {}", torrentDir.string());
            if (!std::filesystem::exists(torrentDir)) {
                getLogger()->debug("Creating torrent files directory: {}", torrentDir.string());
                try {
                    if (!std::filesystem::create_directories(torrentDir)) {
                        getLogger()->error("Failed to create torrent files directory: {}", m_config.torrentFilesDirectory);
                        return false;
                    }
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while creating torrent files directory: {}, error: {}",
                                 m_config.torrentFilesDirectory, e.what());
                    return false;
                }
                getLogger()->debug("Created torrent files directory: {}", torrentDir.string());
            } else {
                getLogger()->debug("Torrent files directory already exists: {}", torrentDir.string());
            }

            // Check if the directory is writable
            try {
                // Try to create a test file
                std::string testFilePath = (torrentDir / "test.tmp").string();
                std::ofstream testFile(testFilePath);
                if (!testFile) {
                    getLogger()->error("Torrent files directory is not writable: {}", torrentDir.string());
                    return false;
                }
                testFile.close();

                // Remove the test file
                std::filesystem::remove(testFilePath);
                getLogger()->debug("Torrent files directory is writable: {}", torrentDir.string());
            } catch (const std::exception& e) {
                getLogger()->error("Exception while testing torrent files directory writability: {}, error: {}",
                             torrentDir.string(), e.what());
                return false;
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while creating torrent files directory: {}", e.what());
            return false;
        }
    }

    // Log configuration
    getLogger()->info("Metadata storage configuration:");
    getLogger()->info("  Storage directory: {}", m_config.storageDirectory);
    getLogger()->info("  Persist metadata: {}", m_config.persistMetadata ? "yes" : "no");
    getLogger()->info("  Create torrent files: {}", m_config.createTorrentFiles ? "yes" : "no");
    getLogger()->info("  Torrent files directory: {}", m_config.torrentFilesDirectory);
    getLogger()->info("  Max metadata items: {}", m_config.maxMetadataItems);
    getLogger()->info("  Deduplicate metadata: {}", m_config.deduplicateMetadata ? "yes" : "no");
    getLogger()->info("  Load metadata on demand: {}", m_config.loadMetadataOnDemand ? "yes" : "no");
    getLogger()->info("  Max load items: {}", m_config.maxLoadItems);
    getLogger()->info("  Max memory usage: {} MB", m_config.maxMemoryUsageMB);

    // Initialize memory usage tracking
    try {
        size_t currentMemoryUsage = getCurrentMemoryUsage();
        getLogger()->info("Initial memory usage: {} MB", currentMemoryUsage / (1024 * 1024));
    } catch (...) {
        getLogger()->warning("Unable to determine initial memory usage");
    }

    // Load all metadata from disk
    if (m_config.persistMetadata) {
        getLogger()->debug("Loading all metadata from disk");
        auto startTime = std::chrono::steady_clock::now();

        if (!loadAllMetadata()) {
            getLogger()->warning("Failed to load all metadata from disk");
            // Continue anyway, this is not critical
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        getLogger()->info("Loaded metadata in {} ms", duration.count());
    }

    m_initialized = true;
    getLogger()->info("Metadata storage initialized with {} items loaded (out of {} known)",
                 m_metadata.size(), m_knownInfoHashes.size());
    return true;
}

void MetadataStorage::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Save all metadata to disk
    if (m_config.persistMetadata) {
        saveAllMetadata();
    }

    // Clear all metadata
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_metadata.clear();
    }

    m_initialized = false;
    getLogger()->info("Metadata storage shut down");
}

bool MetadataStorage::isInitialized() const {
    return m_initialized;
}

bool MetadataStorage::addMetadata(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {

    if (!m_initialized) {
        getLogger()->error("Cannot add metadata: Metadata storage not initialized");
        return false;
    }

    if (metadata.empty() || size == 0) {
        getLogger()->error("Cannot add metadata: Empty metadata");
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we already have this metadata
    if (m_config.deduplicateMetadata) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_metadata.find(infoHashStr) != m_metadata.end()) {
            getLogger()->debug("Metadata already exists for info hash: {}", infoHashStr);
            return true;
        }
    }

    // Extract metadata information
    std::string name;
    uint64_t totalSize;
    bool isMultiFile;
    if (!extractMetadataInfo(metadata, size, name, totalSize, isMultiFile)) {
        getLogger()->error("Failed to extract metadata information for info hash: {}", infoHashStr);
        return false;
    }

    // Create a new metadata item
    MetadataItem item;
    item.infoHash = infoHash;
    item.metadata = metadata;
    item.size = size;
    item.name = name;
    item.totalSize = totalSize;
    item.isMultiFile = isMultiFile;
    item.addedTime = std::chrono::system_clock::now();

    // Add to the storage
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if we need to remove some items to make room
        if (m_metadata.size() >= m_config.maxMetadataItems) {
            // Find the oldest item
            auto oldest = m_metadata.begin();
            for (auto it = m_metadata.begin(); it != m_metadata.end(); ++it) {
                if (it->second.addedTime < oldest->second.addedTime) {
                    oldest = it;
                }
            }

            // Remove the oldest item
            getLogger()->debug("Removing oldest metadata item to make room: {}", oldest->first);
            m_metadata.erase(oldest);
        }

        // Add the new item
        m_metadata[infoHashStr] = item;
    }

    getLogger()->debug("Added metadata for info hash: {}, name: {}, size: {}", infoHashStr, name, totalSize);

    // Save the metadata to disk
    if (m_config.persistMetadata) {
        saveMetadata(infoHash);
    }

    // Create a torrent file
    if (m_config.createTorrentFiles) {
        createTorrentFile(infoHash, metadata, size);
    }

    // Call the callback
    if (m_callback) {
        m_callback(infoHash, metadata, size);
    }

    return true;
}

std::pair<std::vector<uint8_t>, uint32_t> MetadataStorage::getMetadata(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        getLogger()->error("Cannot get metadata: Metadata storage not initialized");
        return {};
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // First, check if we already have the metadata loaded
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_metadata.find(infoHashStr);
        if (it != m_metadata.end()) {
            // Update access time (using const_cast since this doesn't affect the public interface)
            const_cast<MetadataStorage*>(this)->updateAccessTime(infoHashStr);
            return {it->second.metadata, it->second.size};
        }
    }

    // If on-demand loading is enabled, try to load the metadata
    if (m_config.loadMetadataOnDemand && m_config.persistMetadata) {
        // Use const_cast to call the non-const loadMetadataOnDemand method
        // This is safe because we're not modifying the public interface
        if (const_cast<MetadataStorage*>(this)->loadMetadataOnDemand(infoHash)) {
            // Now we should have the metadata loaded
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_metadata.find(infoHashStr);
            if (it != m_metadata.end()) {
                return {it->second.metadata, it->second.size};
            }
        }
    }

    return {};
}

std::shared_ptr<TorrentFile> MetadataStorage::getTorrentFile(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        getLogger()->error("Cannot get torrent file: Metadata storage not initialized");
        return nullptr;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Update access time if we have the metadata loaded
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_metadata.find(infoHashStr);
        if (it != m_metadata.end()) {
            const_cast<MetadataStorage*>(this)->updateAccessTime(infoHashStr);
        }
    }

    // Check if the torrent file exists
    std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
    if (!std::filesystem::exists(torrentFilePath)) {
        // Try to create the torrent file
        auto [metadata, size] = getMetadata(infoHash); // This will trigger on-demand loading if enabled
        if (metadata.empty() || size == 0) {
            getLogger()->debug("No metadata available for info hash: {}", infoHashStr);
            return nullptr;
        }

        const_cast<MetadataStorage*>(this)->createTorrentFile(infoHash, metadata, size);
    }

    // Load the torrent file
    return loadTorrentFromFile(torrentFilePath);
}

bool MetadataStorage::hasMetadata(const std::array<uint8_t, 20>& infoHash) const {
    if (!m_initialized) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // First, check if we already have the metadata loaded
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_metadata.find(infoHashStr) != m_metadata.end()) {
            // Update access time
            const_cast<MetadataStorage*>(this)->updateAccessTime(infoHashStr);
            return true;
        }
    }

    // If on-demand loading is enabled, check if we know about this info hash
    if (m_config.loadMetadataOnDemand && m_config.persistMetadata) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::find(m_knownInfoHashes.begin(), m_knownInfoHashes.end(), infoHashStr) != m_knownInfoHashes.end();
    }

    return false;
}

size_t MetadataStorage::getMetadataCount() const {
    if (!m_initialized) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadata.size();
}

std::vector<std::array<uint8_t, 20>> MetadataStorage::getAllInfoHashes() const {
    if (!m_initialized) {
        return {};
    }

    std::vector<std::array<uint8_t, 20>> infoHashes;

    std::lock_guard<std::mutex> lock(m_mutex);
    infoHashes.reserve(m_metadata.size());

    for (const auto& [infoHashStr, item] : m_metadata) {
        infoHashes.push_back(item.infoHash);
    }

    return infoHashes;
}

std::vector<std::shared_ptr<TorrentFile>> MetadataStorage::getAllTorrentFiles() const {
    if (!m_initialized) {
        return {};
    }

    std::vector<std::shared_ptr<TorrentFile>> torrentFiles;

    std::lock_guard<std::mutex> lock(m_mutex);
    torrentFiles.reserve(m_metadata.size());

    for (const auto& [infoHashStr, item] : m_metadata) {
        auto torrentFile = getTorrentFile(item.infoHash);
        if (torrentFile) {
            torrentFiles.push_back(torrentFile);
        }
    }

    return torrentFiles;
}

bool MetadataStorage::removeMetadata(const std::array<uint8_t, 20>& infoHash) {
    if (!m_initialized) {
        getLogger()->error("Cannot remove metadata: Metadata storage not initialized");
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_metadata.find(infoHashStr);
    if (it == m_metadata.end()) {
        return false;
    }

    // Remove the metadata
    m_metadata.erase(it);

    // Remove the metadata file
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);
            if (std::filesystem::exists(metadataFilePath)) {
                std::filesystem::remove(metadataFilePath);
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while removing metadata file: {}", e.what());
        }
    }

    // Remove the torrent file
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);
            if (std::filesystem::exists(torrentFilePath)) {
                std::filesystem::remove(torrentFilePath);
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while removing torrent file: {}", e.what());
        }
    }

    getLogger()->debug("Removed metadata for info hash: {}", infoHashStr);
    return true;
}

void MetadataStorage::clearMetadata() {
    if (!m_initialized) {
        getLogger()->error("Cannot clear metadata: Metadata storage not initialized");
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear the metadata
    m_metadata.clear();

    // Remove all metadata files
    if (m_config.persistMetadata) {
        try {
            std::filesystem::path storageDir(m_config.storageDirectory);
            if (std::filesystem::exists(storageDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(storageDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".metadata") {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while removing metadata files: {}", e.what());
        }
    }

    // Remove all torrent files
    if (m_config.createTorrentFiles) {
        try {
            std::filesystem::path torrentDir(m_config.torrentFilesDirectory);
            if (std::filesystem::exists(torrentDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(torrentDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".torrent") {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while removing torrent files: {}", e.what());
        }
    }

    getLogger()->info("Cleared all metadata");
}

void MetadataStorage::setCallback(MetadataStorageCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = callback;
}

MetadataStorageConfig MetadataStorage::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void MetadataStorage::setConfig(const MetadataStorageConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

bool MetadataStorage::saveAllMetadata() {
    if (!m_initialized) {
        getLogger()->error("Cannot save metadata: Metadata storage not initialized");
        return false;
    }

    if (!m_config.persistMetadata) {
        getLogger()->warning("Persistence is disabled");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    bool success = true;
    for (const auto& [infoHashStr, item] : m_metadata) {
        if (!saveMetadata(item.infoHash)) {
            success = false;
        }
    }

    getLogger()->info("Saved {} metadata items", m_metadata.size());
    return success;
}

bool MetadataStorage::loadAllMetadata() {
    getLogger()->info("Starting loadAllMetadata - loading metadata from directory: {}", m_config.storageDirectory);

    if (m_initialized) {
        getLogger()->warning("Metadata storage already initialized, but proceeding with loadAllMetadata");
    }

    if (!m_config.persistMetadata) {
        getLogger()->warning("Persistence is disabled, skipping loadAllMetadata");
        return false;
    }

    // Log memory usage before loading
    try {
        size_t currentMemoryUsage = getCurrentMemoryUsage();
        getLogger()->debug("Current memory usage before loading metadata: {} MB", currentMemoryUsage / (1024 * 1024));
    } catch (...) {
        getLogger()->debug("Unable to determine current memory usage");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    getLogger()->debug("Acquired mutex for loadAllMetadata");

    try {
        // Stage 1: Check storage directory
        getLogger()->debug("Stage 1: Checking storage directory");
        std::filesystem::path storageDir(m_config.storageDirectory);
        getLogger()->debug("Storage directory path: {}", storageDir.string());

        if (!std::filesystem::exists(storageDir)) {
            getLogger()->info("Storage directory does not exist: {}, no metadata to load", storageDir.string());
            return true;
        }

        // Stage 2: Scan directory for metadata files
        getLogger()->debug("Stage 2: Scanning directory for metadata files");
        int successCount = 0;

        // Set a timeout for the directory scan
        auto startTime = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(30); // Increased from 5 to 30 seconds

        // Use a vector to store entries to avoid potential issues with directory iterators
        std::vector<std::filesystem::directory_entry> entries;

        // Collect entries with a timeout and progress reporting
        try {
            getLogger()->debug("Starting directory scan with {} second timeout", timeout.count());
            size_t entryCount = 0;
            size_t lastReportedCount = 0;

            for (const auto& entry : std::filesystem::directory_iterator(storageDir)) {
                entries.push_back(entry);
                entryCount++;

                // Report progress every 1000 entries
                if (entryCount % 1000 == 0 && entryCount != lastReportedCount) {
                    getLogger()->debug("Directory scan in progress: {} entries found so far", entryCount);
                    lastReportedCount = entryCount;
                }

                // Check if we've exceeded the timeout
                auto now = std::chrono::steady_clock::now();
                if (now - startTime > timeout) {
                    getLogger()->warning("Directory scan timed out after {} seconds, processing {} entries found so far",
                                   timeout.count(), entries.size());
                    break;
                }
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while scanning directory: {}", e.what());
            // Continue with any entries we've collected so far
        }

        getLogger()->info("Found {} entries in directory {}", entries.size(), storageDir.string());

        // Stage 3: Process metadata files
        getLogger()->debug("Stage 3: Processing metadata files");
        size_t processedCount = 0;
        size_t lastReportedProcessed = 0;
        auto processingStartTime = std::chrono::steady_clock::now();

        // Filter for metadata files and sort by modification time (newest first)
        std::vector<std::filesystem::path> metadataFiles;
        for (const auto& entry : entries) {
            if (entry.is_regular_file() && entry.path().extension() == ".metadata") {
                metadataFiles.push_back(entry.path());
            }
        }

        // Sort by modification time (newest first)
        std::sort(metadataFiles.begin(), metadataFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        });

        getLogger()->info("Found {} metadata files, sorted by modification time", metadataFiles.size());

        // Store all info hashes for on-demand loading
        m_knownInfoHashes.clear();
        for (const auto& path : metadataFiles) {
            std::string filename = path.stem().string();
            if (filename.length() == 40) {
                m_knownInfoHashes.push_back(filename);
            }
        }

        getLogger()->info("Stored {} info hashes for on-demand loading", m_knownInfoHashes.size());

        // If on-demand loading is enabled, only load a limited number of files
        size_t filesToLoad = metadataFiles.size();
        if (m_config.loadMetadataOnDemand) {
            filesToLoad = std::min(filesToLoad, static_cast<size_t>(m_config.maxLoadItems));
            getLogger()->info("On-demand loading enabled, loading only {} newest files", filesToLoad);
        }

        // Process the metadata files
        for (size_t i = 0; i < filesToLoad; i++) {
            const auto& path = metadataFiles[i];

            // Report progress every 100 files or every 5 seconds
            processedCount++;
            auto now = std::chrono::steady_clock::now();
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - processingStartTime).count();

            if ((processedCount % 100 == 0 && processedCount != lastReportedProcessed) ||
                (elapsedSeconds % 5 == 0 && processedCount != lastReportedProcessed)) {
                getLogger()->debug("Processing metadata files: {}/{} ({:.1f}%)",
                             processedCount, filesToLoad,
                             (static_cast<double>(processedCount) / filesToLoad) * 100.0);
                lastReportedProcessed = processedCount;
            }

            // Check memory usage and throttle if necessary
            if (isMemoryUsageAboveThreshold()) {
                getLogger()->warning("Memory usage above threshold ({}MB), throttling metadata loading",
                               m_config.maxMemoryUsageMB);
                // Evict some metadata to free memory
                size_t evicted = evictLeastRecentlyUsedMetadata(10);
                getLogger()->info("Evicted {} metadata items to free memory", evicted);

                // If we still can't free enough memory, stop loading
                if (isMemoryUsageAboveThreshold()) {
                    getLogger()->warning("Memory usage still above threshold after eviction, stopping metadata loading");
                    break;
                }
            }

            // Extract the info hash from the filename
            std::string filename = path.stem().string();

            if (filename.length() != 40) {
                getLogger()->warning("Invalid metadata filename: {}", path.string());
                continue;
            }

            // Convert the info hash string to bytes
            std::array<uint8_t, 20> infoHash;
            try {
                auto bytes = util::hexToBytes(filename);
                if (bytes.size() != 20) {
                    getLogger()->warning("Invalid info hash in filename: {}", filename);
                    continue;
                }
                std::copy(bytes.begin(), bytes.end(), infoHash.begin());
            } catch (const std::exception& e) {
                getLogger()->warning("Failed to parse info hash from filename: {}, error: {}", filename, e.what());
                continue;
            }

            // Load the metadata
            if (!loadMetadata(infoHash)) {
                getLogger()->warning("Failed to load metadata from file: {}", path.string());
            } else {
                successCount++;
                // Update access time
                m_accessTimes[filename] = std::chrono::steady_clock::now();
                // Update loaded item count
                m_loadedItemCount++;
            }
        }

        getLogger()->info("Loaded {} metadata items from {} files ({} total files available)",
                     m_metadata.size(), successCount, metadataFiles.size());

        // Log memory usage after loading
        try {
            size_t currentMemoryUsage = getCurrentMemoryUsage();
            getLogger()->info("Current memory usage after loading metadata: {} MB", currentMemoryUsage / (1024 * 1024));
        } catch (...) {
            getLogger()->debug("Unable to determine current memory usage");
        }

        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading metadata: {}", e.what());
        return false;
    }
}

std::filesystem::path MetadataStorage::getMetadataFilePath(const std::array<uint8_t, 20>& infoHash) const {
    std::string infoHashStr = infoHashToString(infoHash);
    return std::filesystem::path(m_config.storageDirectory) / (infoHashStr + ".metadata");
}

std::filesystem::path MetadataStorage::getTorrentFilePath(const std::array<uint8_t, 20>& infoHash) const {
    std::string infoHashStr = infoHashToString(infoHash);
    return std::filesystem::path(m_config.torrentFilesDirectory) / (infoHashStr + ".torrent");
}

bool MetadataStorage::saveMetadata(const std::array<uint8_t, 20>& infoHash) {
    if (!m_config.persistMetadata) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Find the metadata
    auto it = m_metadata.find(infoHashStr);
    if (it == m_metadata.end()) {
        getLogger()->error("Cannot save metadata: Metadata not found for info hash: {}", infoHashStr);
        return false;
    }

    const auto& item = it->second;

    try {
        // Create the metadata file path
        std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);

        // Create the directory if it doesn't exist
        std::filesystem::path dir = metadataFilePath.parent_path();
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                getLogger()->error("Failed to create directory: {}", dir.string());
                return false;
            }
        }

        // Open the file
        std::ofstream file(metadataFilePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for writing: {}", metadataFilePath.string());
            return false;
        }

        // Write the metadata size
        file.write(reinterpret_cast<const char*>(&item.size), sizeof(item.size));

        // Write the metadata
        file.write(reinterpret_cast<const char*>(item.metadata.data()), static_cast<std::streamsize>(item.metadata.size()));

        getLogger()->debug("Saved metadata to file: {}", metadataFilePath.string());
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving metadata: {}", e.what());
        return false;
    }
}

bool MetadataStorage::loadMetadata(const std::array<uint8_t, 20>& infoHash) {
    std::string infoHashStr = infoHashToString(infoHash);
    getLogger()->debug("Starting loadMetadata for info hash: {}", infoHashStr);

    if (!m_config.persistMetadata) {
        getLogger()->debug("Persistence is disabled, skipping loadMetadata for {}", infoHashStr);
        return false;
    }

    try {
        // Set a timeout for the operation - increased from 2 to 5 seconds
        auto startTime = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(5);
        getLogger()->debug("Using {} second timeout for loading metadata {}", timeout.count(), infoHashStr);

        // Stage 1: Check if the file exists
        getLogger()->debug("Stage 1: Checking if metadata file exists for {}", infoHashStr);
        std::filesystem::path metadataFilePath = getMetadataFilePath(infoHash);
        getLogger()->debug("Metadata file path: {}", metadataFilePath.string());

        if (!std::filesystem::exists(metadataFilePath)) {
            getLogger()->debug("Metadata file does not exist: {}", metadataFilePath.string());
            return false;
        }
        getLogger()->debug("Metadata file exists: {}", metadataFilePath.string());

        // Check file size before opening
        try {
            auto fileSize = std::filesystem::file_size(metadataFilePath);
            getLogger()->debug("Metadata file size: {} bytes", fileSize);

            if (fileSize == 0) {
                getLogger()->warning("Metadata file is empty: {}", metadataFilePath.string());
                return false;
            }

            if (fileSize > 100 * 1024 * 1024) { // 100MB limit
                getLogger()->warning("Metadata file is too large: {} bytes", fileSize);
                return false;
            }
        } catch (const std::exception& e) {
            getLogger()->warning("Failed to get file size: {}, error: {}", metadataFilePath.string(), e.what());
            // Continue anyway, we'll check the size when reading
        }

        // Check if we've exceeded the timeout
        auto now = std::chrono::steady_clock::now();
        if (now - startTime > timeout) {
            getLogger()->warning("Timeout while checking file existence: {}", metadataFilePath.string());
            return false;
        }

        // Stage 2: Open the file
        getLogger()->debug("Stage 2: Opening metadata file for {}", infoHashStr);
        std::ifstream file(metadataFilePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for reading: {}, error: {}",
                         metadataFilePath.string(), strerror(errno));
            return false;
        }
        getLogger()->debug("Successfully opened metadata file for reading: {}", metadataFilePath.string());

        // Check if we've exceeded the timeout
        now = std::chrono::steady_clock::now();
        if (now - startTime > timeout) {
            getLogger()->warning("Timeout while opening file: {}", metadataFilePath.string());
            return false;
        }

        // Stage 3: Read the metadata size
        getLogger()->debug("Stage 3: Reading metadata size for {}", infoHashStr);
        uint32_t size = 0;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));

        if (!file) {
            getLogger()->error("Failed to read metadata size from file: {}, error: {}",
                         metadataFilePath.string(), strerror(errno));
            return false;
        }

        getLogger()->debug("Read metadata size: {} bytes for {}", size, infoHashStr);

        // Validate the size to prevent allocation issues
        if (size == 0) {
            getLogger()->error("Invalid metadata size (zero) in file: {}", metadataFilePath.string());
            return false;
        }

        if (size > 100 * 1024 * 1024) { // Max 100MB
            getLogger()->error("Invalid metadata size (too large): {} bytes in file: {}",
                         size, metadataFilePath.string());
            return false;
        }

        // Check if we've exceeded the timeout
        now = std::chrono::steady_clock::now();
        if (now - startTime > timeout) {
            getLogger()->warning("Timeout while reading metadata size: {}", metadataFilePath.string());
            return false;
        }

        // Stage 4: Read the metadata
        getLogger()->debug("Stage 4: Reading metadata content for {}", infoHashStr);
        std::vector<uint8_t> metadata(size);

        // Read in smaller chunks to avoid potential issues with large files
        const size_t chunkSize = 4096; // 4KB chunks
        size_t bytesRead = 0;

        while (bytesRead < size) {
            size_t remainingBytes = size - bytesRead;
            size_t bytesToRead = std::min(remainingBytes, chunkSize);

            file.read(reinterpret_cast<char*>(metadata.data() + bytesRead), static_cast<std::streamsize>(bytesToRead));

            if (!file && !file.eof()) {
                getLogger()->error("Failed to read metadata chunk from file: {}, error: {}",
                             metadataFilePath.string(), strerror(errno));
                return false;
            }

            size_t chunkBytesRead = static_cast<size_t>(file.gcount());
            bytesRead += chunkBytesRead;

            if (chunkBytesRead < bytesToRead) {
                getLogger()->error("Unexpected end of file while reading metadata: {}, read {} of {} bytes",
                             metadataFilePath.string(), bytesRead, size);
                return false;
            }

            // Check for timeout periodically
            if (bytesRead % (chunkSize * 10) == 0) {
                now = std::chrono::steady_clock::now();
                if (now - startTime > timeout) {
                    getLogger()->warning("Timeout while reading metadata content: {}", metadataFilePath.string());
                    return false;
                }
            }
        }

        getLogger()->debug("Successfully read {} bytes of metadata for {}", bytesRead, infoHashStr);

        // Stage 5: Extract metadata information
        getLogger()->debug("Stage 5: Extracting metadata information for {}", infoHashStr);
        std::string name;
        uint64_t totalSize;
        bool isMultiFile;

        if (!extractMetadataInfo(metadata, size, name, totalSize, isMultiFile)) {
            getLogger()->error("Failed to extract metadata information from file: {}", metadataFilePath.string());
            return false;
        }

        getLogger()->debug("Successfully extracted metadata info: name='{}', size={}, multiFile={}",
                     name, totalSize, isMultiFile);

        // Stage 6: Create metadata item and add to storage
        getLogger()->debug("Stage 6: Adding metadata to storage for {}", infoHashStr);
        MetadataItem item;
        item.infoHash = infoHash;
        item.metadata = std::move(metadata); // Use move to avoid copying the data
        item.size = size;
        item.name = name;
        item.totalSize = totalSize;
        item.isMultiFile = isMultiFile;
        item.addedTime = std::chrono::system_clock::now();

        // Add to the storage
        m_metadata[infoHashStr] = std::move(item); // Use move to avoid copying the item

        getLogger()->info("Successfully loaded metadata from file: {} (name: '{}', size: {})",
                     metadataFilePath.string(), name, totalSize);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading metadata for {}: {}", infoHashStr, e.what());
        return false;
    } catch (...) {
        getLogger()->error("Unknown exception while loading metadata for {}", infoHashStr);
        return false;
    }
}

bool MetadataStorage::createTorrentFile(
    const std::array<uint8_t, 20>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {

    if (!m_config.createTorrentFiles) {
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    try {
        // Create a torrent file builder
        TorrentFileBuilder builder(metadata, size);

        // Check if the builder is valid
        if (!builder.isValid()) {
            getLogger()->error("Failed to create torrent file: Invalid metadata for info hash: {}", infoHashStr);
            return false;
        }

        // Create the torrent file path
        std::filesystem::path torrentFilePath = getTorrentFilePath(infoHash);

        // Create the directory if it doesn't exist
        std::filesystem::path dir = torrentFilePath.parent_path();
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                getLogger()->error("Failed to create directory: {}", dir.string());
                return false;
            }
        }

        // Save the torrent file
        if (!builder.saveTorrentFile(torrentFilePath.string())) {
            getLogger()->error("Failed to save torrent file: {}", torrentFilePath.string());
            return false;
        }

        getLogger()->debug("Created torrent file: {}", torrentFilePath.string());
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while creating torrent file: {}", e.what());
        return false;
    }
}

bool MetadataStorage::extractMetadataInfo(
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    std::string& name,
    uint64_t& totalSize,
    bool& isMultiFile) {

    getLogger()->debug("Extracting metadata information from {} bytes", size);

    try {
        // Create a torrent file builder
        getLogger()->debug("Creating TorrentFileBuilder");
        TorrentFileBuilder builder(metadata, size);

        // Check if the builder is valid
        if (!builder.isValid()) {
            getLogger()->error("Failed to extract metadata information: Invalid metadata");
            return false;
        }
        getLogger()->debug("TorrentFileBuilder is valid");

        // Get the metadata information
        name = builder.getName();
        totalSize = builder.getTotalSize();
        isMultiFile = builder.isMultiFile();

        getLogger()->debug("Extracted metadata information: name={}, totalSize={}, isMultiFile={}",
                     name, totalSize, isMultiFile);

        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while extracting metadata information: {}", e.what());
        return false;
    }
}

std::string MetadataStorage::infoHashToString(const std::array<uint8_t, 20>& infoHash) const {
    return util::bytesToHex(infoHash.data(), infoHash.size());
}

size_t MetadataStorage::getCurrentMemoryUsage() const {
    // Get the current memory usage of the process
    size_t memoryUsage = 0;

#ifdef __APPLE__
    // macOS implementation
    struct task_basic_info info;
    mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                 reinterpret_cast<task_info_t>(&info), &infoCount) == KERN_SUCCESS) {
        memoryUsage = info.resident_size;
    }
#elif defined(_WIN32)
    // Windows implementation
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        memoryUsage = pmc.WorkingSetSize;
    }
#else
    // Linux implementation
    FILE* file = fopen("/proc/self/statm", "r");
    if (file) {
        unsigned long size, resident, share, text, lib, data, dt;
        if (fscanf(file, "%lu %lu %lu %lu %lu %lu %lu", &size, &resident, &share, &text, &lib, &data, &dt) == 7) {
            memoryUsage = resident * sysconf(_SC_PAGESIZE);
        }
        fclose(file);
    }
#endif

    return memoryUsage;
}

bool MetadataStorage::loadMetadataOnDemand(const std::array<uint8_t, 20>& infoHash) {
    if (!m_initialized) {
        getLogger()->error("Cannot load metadata on demand: Metadata storage not initialized");
        return false;
    }

    if (!m_config.persistMetadata) {
        getLogger()->debug("Persistence is disabled, cannot load metadata on demand");
        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we already have this metadata
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_metadata.find(infoHashStr) != m_metadata.end()) {
            // Update access time
            m_accessTimes[infoHashStr] = std::chrono::steady_clock::now();
            getLogger()->debug("Metadata already loaded for info hash: {}", infoHashStr);
            return true;
        }
    }

    // Check if we know about this info hash
    bool knownInfoHash = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        knownInfoHash = std::find(m_knownInfoHashes.begin(), m_knownInfoHashes.end(), infoHashStr) != m_knownInfoHashes.end();
    }

    if (!knownInfoHash) {
        getLogger()->debug("Info hash not known: {}", infoHashStr);
        return false;
    }

    // Check memory usage and evict if necessary
    if (isMemoryUsageAboveThreshold()) {
        getLogger()->debug("Memory usage above threshold, evicting least recently used metadata");
        evictLeastRecentlyUsedMetadata(5); // Evict 5 items
    }

    // Load the metadata
    getLogger()->debug("Loading metadata on demand for info hash: {}", infoHashStr);
    if (!loadMetadata(infoHash)) {
        getLogger()->warning("Failed to load metadata on demand for info hash: {}", infoHashStr);
        return false;
    }

    // Update access time and loaded item count
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accessTimes[infoHashStr] = std::chrono::steady_clock::now();
        m_loadedItemCount++;
    }

    getLogger()->debug("Successfully loaded metadata on demand for info hash: {}", infoHashStr);
    return true;
}

bool MetadataStorage::isMemoryUsageAboveThreshold() const {
    try {
        size_t currentMemoryUsage = getCurrentMemoryUsage();
        size_t thresholdBytes = static_cast<size_t>(m_config.maxMemoryUsageMB) * 1024 * 1024;
        return currentMemoryUsage > thresholdBytes;
    } catch (...) {
        getLogger()->warning("Unable to determine memory usage, assuming below threshold");
        return false;
    }
}

size_t MetadataStorage::evictLeastRecentlyUsedMetadata(size_t count) {
    if (!m_initialized) {
        getLogger()->error("Cannot evict metadata: Metadata storage not initialized");
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // If we have fewer items than the count, don't evict anything
    if (m_metadata.size() <= count) {
        getLogger()->debug("Not enough metadata items to evict ({} items, requested {})", m_metadata.size(), count);
        return 0;
    }

    // Create a vector of pairs (info hash, access time)
    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> accessTimes;
    for (const auto& [infoHash, accessTime] : m_accessTimes) {
        accessTimes.emplace_back(infoHash, accessTime);
    }

    // Sort by access time (oldest first)
    std::sort(accessTimes.begin(), accessTimes.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    // Evict the oldest items
    size_t evicted = 0;
    for (size_t i = 0; i < count && i < accessTimes.size(); i++) {
        const auto& infoHash = accessTimes[i].first;

        // Skip if not in metadata (shouldn't happen, but just in case)
        if (m_metadata.find(infoHash) == m_metadata.end()) {
            continue;
        }

        // Evict the metadata
        m_metadata.erase(infoHash);
        m_accessTimes.erase(infoHash);
        evicted++;

        getLogger()->debug("Evicted metadata for info hash: {}", infoHash);
    }

    // Update loaded item count
    m_loadedItemCount -= evicted;

    getLogger()->info("Evicted {} metadata items", evicted);
    return evicted;
}

std::vector<std::filesystem::path> MetadataStorage::getAllMetadataFilePaths() const {
    std::vector<std::filesystem::path> paths;

    if (!m_config.persistMetadata) {
        return paths;
    }

    try {
        std::filesystem::path storageDir(m_config.storageDirectory);
        if (!std::filesystem::exists(storageDir)) {
            return paths;
        }

        for (const auto& entry : std::filesystem::directory_iterator(storageDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".metadata") {
                paths.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while getting metadata file paths: {}", e.what());
    }

    return paths;
}

void MetadataStorage::updateAccessTime(const std::string& infoHashStr) {
    // No need to lock here since this is called from methods that already have the lock
    m_accessTimes[infoHashStr] = std::chrono::steady_clock::now();
}

} // namespace dht_hunter::metadata
