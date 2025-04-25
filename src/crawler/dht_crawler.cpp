#include "dht_hunter/crawler/dht_crawler.hpp"
#include "dht_hunter/util/hash.hpp"
#include "dht_hunter/util/filesystem_utils.hpp"
#include "dht_hunter/util/process_utils.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_map>
#include <mutex>

DEFINE_COMPONENT_LOGGER("Crawler", "DHTCrawler")

namespace dht_hunter::crawler {
DHTCrawler::DHTCrawler(const DHTCrawlerConfig& config)
    : m_config(config),
      m_metadataFetchQueue() {
}
DHTCrawler::~DHTCrawler() {
    stop();
}
bool DHTCrawler::start() {
    if (m_running) {
        getLogger()->warning("DHTCrawler is already running");
        return false;
    }
    getLogger()->info("Starting DHTCrawler");
    // Initialize components
    getLogger()->info("Initializing DHT node");
    getLogger()->debug("Calling initializeDHTNode()");
    bool dhtNodeInitialized = initializeDHTNode();
    getLogger()->debug("initializeDHTNode() returned: {}", dhtNodeInitialized ? "true" : "false");

    if (!dhtNodeInitialized) {
        getLogger()->error("Failed to initialize DHT node");
        return false;
    }

    getLogger()->debug("DHT node initialized successfully");
    if (m_config.useInfoHashCollector) {
        getLogger()->info("Initializing info hash collector");
        if (!initializeInfoHashCollector()) {
            getLogger()->error("Failed to initialize info hash collector");
            m_dhtNode->stop();
            return false;
        }
    } else {
        getLogger()->info("Info hash collector disabled");
    }
    getLogger()->info("Initializing metadata fetcher");
    if (!initializeMetadataFetcher()) {
        getLogger()->error("Failed to initialize metadata fetcher");
        if (m_infoHashCollector) {
            m_infoHashCollector->stop();
        }
        m_dhtNode->stop();
        return false;
    }
    if (m_config.useMetadataStorage) {
        getLogger()->info("Initializing metadata storage");
        if (!initializeMetadataStorage()) {
            getLogger()->warning("Failed to initialize metadata storage, continuing without it");
            m_metadataStorageAvailable = false;
        } else {
            m_metadataStorageAvailable = true;
        }
    } else {
        getLogger()->info("Metadata storage disabled");
        m_metadataStorageAvailable = false;
    }
    // Set the running flag
    m_running = true;
    // Start threads
    getLogger()->info("Starting lookup thread");
    m_lookupThread = std::thread(&DHTCrawler::lookupThread, this);
    getLogger()->info("Starting metadata fetch thread");
    m_metadataFetchThread = std::thread(&DHTCrawler::metadataFetchThread, this);
    getLogger()->info("Starting status thread");
    m_statusThread = std::thread(&DHTCrawler::statusThread, this);

    // Start active node discovery thread if enabled
    if (m_config.enableActiveNodeDiscovery) {
        getLogger()->info("Starting active node discovery thread");
        m_activeNodeDiscoveryThread = std::thread(&DHTCrawler::activeNodeDiscoveryThread, this);
    } else {
        getLogger()->info("Active node discovery disabled");
    }
    getLogger()->info("DHTCrawler started");
    return true;
}
void DHTCrawler::stop() {
    if (!m_running) {
        return;
    }
    // Set the running flag to false to stop threads
    m_running = false;

    // Define a helper function to join a thread with timeout
    auto joinThreadWithTimeout = [](std::thread& thread, const std::string& threadName, std::chrono::seconds timeout) {
        if (!thread.joinable()) {
            return;
        }

        getLogger()->debug("Waiting for {} thread to join...", threadName);

        // Try to join with timeout
        std::thread tempThread;
        bool joined = false;

        // Create a thread to join the thread with timeout
        tempThread = std::thread([&thread, &joined]() {
            if (thread.joinable()) {
                thread.join();
                joined = true;
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            auto joinStartTime = std::chrono::steady_clock::now();
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("{} thread joined successfully", threadName);
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("{} thread join timed out after {} seconds, detaching",
                              threadName, timeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (thread.joinable()) {
                    thread.detach();
                }
            }
        }
    };

    // Join threads with timeout
    const auto threadJoinTimeout = std::chrono::seconds(2);
    joinThreadWithTimeout(m_lookupThread, "lookup", threadJoinTimeout);
    joinThreadWithTimeout(m_metadataFetchThread, "metadata fetch", threadJoinTimeout);
    joinThreadWithTimeout(m_statusThread, "status", threadJoinTimeout);
    joinThreadWithTimeout(m_activeNodeDiscoveryThread, "active node discovery", threadJoinTimeout);

    // Stop components in reverse order of initialization
    try {
        if (m_metadataStorage) {
            getLogger()->debug("Shutting down metadata storage...");
            // The new MetadataStorage class doesn't need explicit shutdown
            m_metadataStorage.reset();
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while shutting down metadata storage: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while shutting down metadata storage");
    }

    try {
        if (m_metadataFetcher) {
            getLogger()->debug("Stopping metadata fetcher...");
            m_metadataFetcher->stop();
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while stopping metadata fetcher: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while stopping metadata fetcher");
    }

    try {
        if (m_infoHashCollector) {
            getLogger()->debug("Stopping info hash collector...");
            m_infoHashCollector->stop();
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while stopping info hash collector: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while stopping info hash collector");
    }

    try {
        if (m_dhtNode) {
            getLogger()->debug("Stopping DHT node...");
            m_dhtNode->stop();
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while stopping DHT node: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while stopping DHT node");
    }

    getLogger()->info("DHTCrawler stopped");
}
bool DHTCrawler::isRunning() const {
    return m_running;
}
std::shared_ptr<dht::DHTNode> DHTCrawler::getDHTNode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dhtNode;
}
std::shared_ptr<InfoHashCollector> DHTCrawler::getInfoHashCollector() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_infoHashCollector;
}
std::shared_ptr<metadata::MetadataFetcher> DHTCrawler::getMetadataFetcher() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataFetcher;
}
std::shared_ptr<storage::MetadataStorage> DHTCrawler::getMetadataStorage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataStorage;
}

bool DHTCrawler::isMetadataStorageAvailable() const {
    return m_metadataStorageAvailable;
}
DHTCrawlerConfig DHTCrawler::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}
void DHTCrawler::setConfig(const DHTCrawlerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}
void DHTCrawler::setStatusCallback(CrawlerStatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCallback = callback;
}
uint64_t DHTCrawler::getInfoHashesDiscovered() const {
    return m_infoHashesDiscovered;
}
uint64_t DHTCrawler::getInfoHashesQueued() const {
    return m_infoHashesQueued;
}
uint64_t DHTCrawler::getMetadataFetched() const {
    return m_metadataFetched;
}
double DHTCrawler::getLookupRate() const {
    return m_lookupRate;
}
double DHTCrawler::getMetadataFetchRate() const {
    return m_metadataFetchRate;
}

uint64_t DHTCrawler::getTotalInfoHashes() const {
    if (m_infoHashCollector) {
        return m_infoHashCollector->getInfoHashCount();
    }
    return 0;
}
bool DHTCrawler::initializeDHTNode() {
    try {
        // Ensure the config directory exists
        getLogger()->info("Ensuring config directory exists: {}", m_config.configDir);
        if (!util::FilesystemUtils::ensureDirectoryExists(m_config.configDir)) {
            getLogger()->error("Failed to create config directory: {}", m_config.configDir);
            return false;
        }

        // Create the DHT node with the configured parameters
        getLogger()->info("Creating DHT node on port {} with config directory {}", m_config.dhtPort, m_config.configDir);

        // Create the DHT node config
        dht::DHTNodeConfig dhtNodeConfig;
        dhtNodeConfig.setPort(m_config.dhtPort);
        dhtNodeConfig.setConfigDir(m_config.configDir);
        dhtNodeConfig.setKBucketSize(m_config.kBucketSize);
        dhtNodeConfig.setLookupAlpha(m_config.lookupAlpha);
        dhtNodeConfig.setLookupMaxResults(m_config.lookupMaxResults);
        dhtNodeConfig.setSaveRoutingTableOnNewNode(m_config.saveRoutingTableOnNewNode);

        // Create the DHT node with the config
        m_dhtNode = std::make_shared<dht::DHTNode>(dhtNodeConfig);

        // Start the DHT node
        getLogger()->debug("Starting DHT node");
        if (!m_dhtNode->start()) {
            getLogger()->error("Failed to start DHT node on port {}", m_config.dhtPort);
            return false;
        }

        getLogger()->info("DHT node started successfully on port {}", m_dhtNode->getPort());
        getLogger()->debug("Preparing to bootstrap DHT node");

        // Debug: Check if the routing table was loaded
        getLogger()->debug("Routing table loaded, checking bootstrap nodes");

        // Debug: Print bootstrap nodes
        getLogger()->debug("Bootstrap nodes:");
        for (const auto& node : m_config.bootstrapNodes) {
            getLogger()->debug("  {}", node);
        }

        // Set a timeout for the entire bootstrap process
        auto bootstrapStartTime = std::chrono::steady_clock::now();
        auto bootstrapTimeout = std::chrono::seconds(30); // 30 second timeout for the entire bootstrap process

        // Bootstrap the DHT node
        getLogger()->info("Bootstrapping DHT node with {} bootstrap nodes (timeout: {} seconds)",
                     m_config.bootstrapNodes.size(), bootstrapTimeout.count());

        getLogger()->debug("Bootstrap process starting at {}",
                     std::chrono::duration_cast<std::chrono::milliseconds>(bootstrapStartTime.time_since_epoch()).count());

        // Debug: Check if the bootstrap nodes are empty
        if (m_config.bootstrapNodes.empty()) {
            getLogger()->warning("No bootstrap nodes configured, skipping bootstrap process");
            getLogger()->info("DHT node initialization complete on port {}", m_dhtNode->getPort());
            getLogger()->debug("Returning from initializeDHTNode() without bootstrapping");
            return true;
        }

        // Debug: Print bootstrap nodes
        getLogger()->debug("Bootstrap nodes:");
        for (const auto& node : m_config.bootstrapNodes) {
            getLogger()->debug("  {}", node);
        }

        // Track bootstrap success
        int successfulBootstraps = 0;
        int totalAttempts = 0;

        // Fallback IP addresses for common bootstrap nodes in case DNS resolution fails
        const std::unordered_map<std::string, std::vector<std::string>> fallbackIPs = {
            {"router.bittorrent.com", {"67.215.246.10", "67.215.246.11"}},
            {"dht.transmissionbt.com", {"212.129.33.59"}},
            {"router.utorrent.com", {"67.215.246.10"}}
        };

        // Try to bootstrap with each configured node
        getLogger()->debug("Starting bootstrap process with {} nodes", m_config.bootstrapNodes.size());
        for (const auto& bootstrapNode : m_config.bootstrapNodes) {
            getLogger()->debug("Processing bootstrap node: {}", bootstrapNode);
            // Check if we've exceeded the bootstrap timeout
            if (std::chrono::steady_clock::now() - bootstrapStartTime > bootstrapTimeout) {
                getLogger()->warning("Bootstrap process timed out after {} seconds, continuing with {} successful bootstraps",
                               bootstrapTimeout.count(), successfulBootstraps);
                break;
            }

            getLogger()->debug("Processing bootstrap node: {}", bootstrapNode);

            // Parse the bootstrap node string (host:port)
            std::string host;
            uint16_t port;

            const size_t colonPos = bootstrapNode.find(':');
            if (colonPos != std::string::npos) {
                host = bootstrapNode.substr(0, colonPos);
                try {
                    port = static_cast<uint16_t>(std::stoi(bootstrapNode.substr(colonPos + 1)));
                    getLogger()->debug("Parsed bootstrap node: host={}, port={}", host, port);
                } catch (const std::exception& e) {
                    getLogger()->warning("Invalid port in bootstrap node {}: {}", bootstrapNode, e.what());
                    continue;
                }
            } else {
                host = bootstrapNode;
                port = 6881; // Default DHT port
                getLogger()->debug("Using default port for bootstrap node: host={}, port={}", host, port);
            }

            // Try to resolve the hostname with a short timeout
            getLogger()->debug("Resolving bootstrap node: {}", host);
            std::vector<network::NetworkAddress> addresses;

            // First try to resolve the hostname with a timeout
            try {
                auto resolveStartTime = std::chrono::steady_clock::now();
                auto resolveTimeout = std::chrono::seconds(2); // 2 second timeout for DNS resolution

                // Use a separate thread for DNS resolution to implement a timeout
                std::atomic<bool> resolutionComplete = false;
                std::thread resolveThread([&]() {
                    try {
                        getLogger()->debug("DNS resolution thread started for {}", host);
                        addresses = network::AddressResolver::resolve(host);
                        getLogger()->debug("DNS resolution successful for {}, got {} addresses", host, addresses.size());

                        // Log the resolved addresses
                        for (const auto& address : addresses) {
                            getLogger()->debug("Resolved {} to {}", host, address.toString());
                        }

                        resolutionComplete = true;
                    } catch (const std::exception& e) {
                        getLogger()->warning("Exception resolving bootstrap node {}: {}", host, e.what());
                        resolutionComplete = true;
                    }
                });

                // Wait for resolution to complete or timeout
                while (!resolutionComplete) {
                    if (std::chrono::steady_clock::now() - resolveStartTime > resolveTimeout) {
                        getLogger()->warning("DNS resolution timed out for {}", host);
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                // Clean up the thread
                if (resolveThread.joinable()) {
                    resolveThread.detach(); // Let it finish in the background
                }

                // Check if we've exceeded the bootstrap timeout
                if (std::chrono::steady_clock::now() - bootstrapStartTime > bootstrapTimeout) {
                    getLogger()->warning("Bootstrap process timed out during DNS resolution, continuing with {} successful bootstraps",
                                   successfulBootstraps);
                    break;
                }
            } catch (const std::exception& e) {
                getLogger()->warning("Exception during DNS resolution thread for {}: {}", host, e.what());
            }

            // If resolution failed, try fallback IPs
            if (addresses.empty()) {
                getLogger()->warning("DNS resolution failed for {}, trying fallback IPs", host);
                auto it = fallbackIPs.find(host);
                if (it != fallbackIPs.end()) {
                    for (const auto& ip : it->second) {
                        getLogger()->debug("Using fallback IP {} for {}", ip, host);
                        addresses.push_back(network::NetworkAddress(ip));
                    }
                }
            }

            if (addresses.empty()) {
                getLogger()->warning("Failed to resolve bootstrap node and no fallbacks available: {}", host);
                continue;
            }

            // Try each resolved address with a timeout
            bool nodeBootstrapped = false;
            for (const auto& address : addresses) {
                // Check if we've exceeded the bootstrap timeout
                if (std::chrono::steady_clock::now() - bootstrapStartTime > bootstrapTimeout) {
                    getLogger()->warning("Bootstrap process timed out during bootstrap attempts, continuing with {} successful bootstraps",
                                   successfulBootstraps);
                    break;
                }

                network::EndPoint endpoint(address, port);
                totalAttempts++;

                getLogger()->debug("Attempting to bootstrap with {}", endpoint.toString());
                getLogger()->debug("Bootstrap attempt #{} for {}", totalAttempts, bootstrapNode);

                // Set a timeout for this specific bootstrap attempt
                auto attemptStartTime = std::chrono::steady_clock::now();
                auto attemptTimeout = std::chrono::seconds(5); // 5 second timeout per bootstrap attempt

                // Use a separate thread for bootstrap to implement a timeout
                std::atomic<bool> bootstrapComplete = false;
                std::atomic<bool> bootstrapSuccess = false;

                getLogger()->debug("Starting bootstrap thread for {}", endpoint.toString());
                std::thread bootstrapThread([&]() {
                    getLogger()->debug("Bootstrap thread started for {}", endpoint.toString());
                    bootstrapSuccess = m_dhtNode->bootstrap(endpoint);
                    getLogger()->debug("Bootstrap thread completed for {}, result: {}",
                                 endpoint.toString(), bootstrapSuccess.load() ? "success" : "failure");
                    bootstrapComplete = true;
                });

                // Wait for bootstrap to complete or timeout
                getLogger()->debug("Waiting for bootstrap to complete for {}", endpoint.toString());
                while (!bootstrapComplete) {
                    if (std::chrono::steady_clock::now() - attemptStartTime > attemptTimeout) {
                        getLogger()->warning("Bootstrap attempt timed out for {}", endpoint.toString());
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                getLogger()->debug("Bootstrap wait completed for {}, complete: {}",
                             endpoint.toString(), bootstrapComplete.load() ? "yes" : "no");

                // Clean up the thread
                if (bootstrapThread.joinable()) {
                    getLogger()->debug("Detaching bootstrap thread for {}", endpoint.toString());
                    bootstrapThread.detach(); // Let it finish in the background
                }

                if (bootstrapComplete && bootstrapSuccess) {
                    getLogger()->info("Successfully bootstrapped with {}", endpoint.toString());
                    successfulBootstraps++;
                    nodeBootstrapped = true;
                    getLogger()->debug("Breaking out of address loop after successful bootstrap with {}", endpoint.toString());
                    break; // Successfully bootstrapped with this node, try the next one
                } else {
                    getLogger()->debug("Failed to bootstrap with {}", endpoint.toString());
                    if (!bootstrapComplete) {
                        getLogger()->debug("Bootstrap did not complete for {}", endpoint.toString());
                    } else {
                        getLogger()->debug("Bootstrap completed but failed for {}", endpoint.toString());
                    }
                }
            }

            // Log result for this bootstrap node
            if (!nodeBootstrapped) {
                getLogger()->warning("Failed to bootstrap with any address for {}", host);
            } else {
                getLogger()->info("Successfully bootstrapped with at least one address for {}", host);
            }

            // If we've bootstrapped with at least 1 node, that's sufficient
            // Changed from 2 to 1 to speed up the process
            if (successfulBootstraps >= 1) {
                getLogger()->info("Successfully bootstrapped with {} node, continuing", successfulBootstraps);
                getLogger()->debug("Breaking out of bootstrap node loop after {} successful bootstraps", successfulBootstraps);
                break;
            } else {
                getLogger()->debug("Continuing to next bootstrap node after {} successful bootstraps", successfulBootstraps);
            }
        }

        // Even if bootstrap fails, we can still continue - the DHT node will eventually
        // discover other nodes through normal DHT traffic
        if (successfulBootstraps == 0) {
            if (totalAttempts > 0) {
                getLogger()->warning("Failed to bootstrap with any nodes ({} attempts), but continuing anyway", totalAttempts);
            } else {
                getLogger()->warning("No bootstrap attempts were made, DHT may have limited connectivity");
            }
        } else {
            getLogger()->info("DHT node bootstrapped successfully with {} out of {} attempts",
                         successfulBootstraps, totalAttempts);
        }

        // Log the total bootstrap time
        auto bootstrapEndTime = std::chrono::steady_clock::now();
        auto bootstrapDuration = std::chrono::duration_cast<std::chrono::milliseconds>(bootstrapEndTime - bootstrapStartTime);
        getLogger()->info("Bootstrap process completed in {} ms", bootstrapDuration.count());

        getLogger()->debug("Bootstrap process ending at {}",
                     std::chrono::duration_cast<std::chrono::milliseconds>(bootstrapEndTime.time_since_epoch()).count());

        getLogger()->info("DHT node initialization complete on port {}", m_dhtNode->getPort());
        getLogger()->debug("Returning from initializeDHTNode() with success");
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception during DHT node initialization: {}", e.what());
        getLogger()->debug("Returning from initializeDHTNode() with failure due to exception");
        return false;
    } catch (...) {
        getLogger()->error("Unknown exception during DHT node initialization");
        getLogger()->debug("Returning from initializeDHTNode() with failure due to unknown exception");
        return false;
    }
}
bool DHTCrawler::initializeInfoHashCollector() {
    // Ensure the info hash collector's save directory exists
    std::filesystem::path savePath(m_config.infoHashCollectorConfig.savePath);
    std::filesystem::path saveDir = savePath.parent_path();
    getLogger()->info("Ensuring info hash collector save directory exists: {}", saveDir.string());
    if (!util::FilesystemUtils::ensureDirectoryExists(saveDir)) {
        getLogger()->error("Failed to create info hash collector save directory: {}", saveDir.string());
        return false;
    }

    // Create the info hash collector
    m_infoHashCollector = std::make_shared<InfoHashCollector>(m_config.infoHashCollectorConfig);

    // Try to load existing infohashes from file
    if (std::filesystem::exists(m_config.infoHashCollectorConfig.savePath)) {
        getLogger()->info("Loading existing infohashes from: {}", m_config.infoHashCollectorConfig.savePath);
        if (!m_infoHashCollector->loadInfoHashes(m_config.infoHashCollectorConfig.savePath)) {
            getLogger()->warning("Failed to load existing infohashes, starting with empty collection");
        }
    } else {
        getLogger()->info("No existing infohashes file found at: {}", m_config.infoHashCollectorConfig.savePath);
    }

    // Set the callback for new info hashes
    m_infoHashCollector->setNewInfoHashCallback(
        [this](const dht::InfoHash& infoHash) {
            handleNewInfoHash(infoHash);
        });
    // Start the info hash collector
    if (!m_infoHashCollector->start()) {
        getLogger()->error("Failed to start info hash collector");
        return false;
    }
    // Set the info hash collector in the DHT node
    m_dhtNode->setInfoHashCollector(m_infoHashCollector);
    getLogger()->info("Info hash collector initialized");
    return true;
}
bool DHTCrawler::initializeMetadataFetcher() {
    // Set client identification in the metadata fetcher config
    auto fetcherConfig = m_config.metadataFetcherConfig;
    fetcherConfig.peerIdPrefix = m_config.peerIdPrefix;
    fetcherConfig.clientVersion = m_config.clientVersion;

    // Create the metadata fetcher
    m_metadataFetcher = std::make_shared<metadata::MetadataFetcher>(fetcherConfig);

    // Disable metadata storage in the fetcher since we'll handle it separately
    auto config = m_metadataFetcher->getConfig();
    config.useMetadataStorage = false;
    m_metadataFetcher->setConfig(config);

    // Start the metadata fetcher
    if (!m_metadataFetcher->start()) {
        getLogger()->error("Failed to start metadata fetcher");
        return false;
    }
    getLogger()->info("Metadata fetcher initialized");
    return true;
}
bool DHTCrawler::initializeMetadataStorage() {
    getLogger()->info("Creating metadata storage");

    // Log the metadata storage configuration
    getLogger()->debug("Metadata storage configuration:");
    getLogger()->debug("  Storage directory: {}", m_config.metadataStorageDirectory);

    // Ensure the metadata storage directory exists
    getLogger()->info("Ensuring metadata storage directory exists: {}", m_config.metadataStorageDirectory);
    if (!util::FilesystemUtils::ensureDirectoryExists(m_config.metadataStorageDirectory)) {
        getLogger()->error("Failed to create metadata storage directory: {}", m_config.metadataStorageDirectory);
        return false;
    }

    // Create the metadata storage
    try {
        getLogger()->debug("Creating MetadataStorage instance");
        m_metadataStorage = std::make_shared<storage::MetadataStorage>(m_config.metadataStorageDirectory);
        getLogger()->debug("MetadataStorage instance created successfully");
    } catch (const std::exception& e) {
        getLogger()->error("Exception while creating metadata storage: {}", e.what());
        return false;
    }

    // Initialize the metadata storage with a timeout
    getLogger()->info("Initializing metadata storage");

    // Set a timeout for initialization (60 seconds - increased from 5 seconds)
    const auto timeout = std::chrono::seconds(60);
    std::atomic<bool> initializationComplete = false;
    std::atomic<bool> initializationSuccess = false;
    std::string initializationErrorMessage = "";
    std::mutex errorMessageMutex;
    std::atomic<int> initializationStage{0};

    // Create a thread to handle the initialization
    std::thread initializationThread([this, &initializationComplete, &initializationSuccess,
                                     &initializationErrorMessage, &errorMessageMutex, &initializationStage]() {
        getLogger()->debug("Metadata storage initialization thread started");
        try {
            // Stage 1: Preparing to initialize
            initializationStage = 1;
            getLogger()->debug("Stage 1: Preparing to initialize metadata storage");

            // Stage 2: Checking if metadata storage was created successfully
            initializationStage = 2;
            getLogger()->debug("Stage 2: Checking if MetadataStorage was created successfully");

            // The new MetadataStorage class is initialized in the constructor
            if (!m_metadataStorage) {
                std::string errorMsg = "MetadataStorage was not created successfully";
                {
                    std::lock_guard<std::mutex> lock(errorMessageMutex);
                    initializationErrorMessage = errorMsg;
                }
                getLogger()->error(errorMsg);
                initializationSuccess = false;
            } else {
                // Stage 3: Initialize completed successfully
                initializationStage = 3;
                getLogger()->debug("Stage 3: MetadataStorage::initialize() returned true");
                initializationSuccess = true;
            }
        } catch (const std::exception& e) {
            std::string errorMsg = "Exception during metadata storage initialization (stage " +
                                  std::to_string(initializationStage.load()) + "): " + e.what();
            {
                std::lock_guard<std::mutex> lock(errorMessageMutex);
                initializationErrorMessage = errorMsg;
            }
            getLogger()->error(errorMsg);
            initializationSuccess = false;
        } catch (...) {
            std::string errorMsg = "Unknown exception during metadata storage initialization (stage " +
                                  std::to_string(initializationStage.load()) + ")";
            {
                std::lock_guard<std::mutex> lock(errorMessageMutex);
                initializationErrorMessage = errorMsg;
            }
            getLogger()->error(errorMsg);
            initializationSuccess = false;
        }

        getLogger()->debug("Metadata storage initialization thread completed with result: {}",
                     initializationSuccess.load() ? "success" : "failure");
        initializationComplete = true;
    });

    // Wait for initialization to complete or timeout
    getLogger()->debug("Waiting for metadata storage initialization to complete (timeout: {} seconds)",
                 timeout.count());
    auto startTime = std::chrono::steady_clock::now();

    // Check progress every 100ms
    while (!initializationComplete) {
        auto elapsedTime = std::chrono::steady_clock::now() - startTime;
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count();

        // Print a progress message every second
        if (elapsedSeconds > 0 && elapsedSeconds % 1 == 0) {
            int currentStage = initializationStage.load();
            std::string stageName;

            switch (currentStage) {
                case 1: stageName = "preparing"; break;
                case 2: stageName = "initializing"; break;
                case 3: stageName = "completing"; break;
                default: stageName = "unknown"; break;
            }

            getLogger()->debug("Metadata storage initialization in progress... (stage: {}, {} seconds elapsed)",
                         stageName, elapsedSeconds);
        }

        // Check if we've exceeded the timeout
        if (elapsedTime > timeout) {
            getLogger()->error("Metadata storage initialization timed out after {} seconds (stage: {})",
                         timeout.count(), initializationStage.load());

            // Don't detach the thread, try to join it with a short timeout
            if (initializationThread.joinable()) {
                getLogger()->debug("Attempting to join initialization thread with 1 second timeout");
                std::thread joinThread([&initializationThread]() {
                    initializationThread.join();
                });

                // Wait for 1 second for the join thread to complete
                auto joinStartTime = std::chrono::steady_clock::now();

                while (joinThread.joinable() &&
                       std::chrono::steady_clock::now() - joinStartTime < std::chrono::seconds(1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                if (joinThread.joinable()) {
                    getLogger()->debug("Join thread did not complete in time, detaching initialization thread");
                    joinThread.detach();
                    initializationThread.detach();
                } else {
                    getLogger()->debug("Successfully joined initialization thread");
                }
            }

            // Try to continue without metadata storage
            getLogger()->warning("Continuing without metadata storage due to initialization timeout");
            return false;
        }

        // Sleep for a short time before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Join the initialization thread
    if (initializationThread.joinable()) {
        getLogger()->debug("Joining initialization thread");
        initializationThread.join();
        getLogger()->debug("Initialization thread joined");
    }

    // Check if initialization was successful
    if (!initializationSuccess) {
        getLogger()->error("Metadata storage initialization failed");
        return false;
    }

    // The new MetadataStorage class is always initialized after construction
    if (!m_metadataStorage) {
        getLogger()->error("Metadata storage is not available");
        return false;
    }

    getLogger()->debug("Metadata storage initialization was successful");

    // Set the metadata storage in the metadata fetcher
    if (m_metadataFetcher) {
        getLogger()->debug("Setting metadata storage in metadata fetcher");
        try {
            m_metadataFetcher->setMetadataStorage(m_metadataStorage);
            getLogger()->info("Set metadata storage in metadata fetcher");
        } catch (const std::exception& e) {
            getLogger()->error("Exception while setting metadata storage in fetcher: {}", e.what());
            return false;
        }
    } else {
        getLogger()->warning("Metadata fetcher is null, cannot set metadata storage");
    }

    // Get metadata count
    size_t metadataCount = 0;
    try {
        metadataCount = m_metadataStorage->count();
    } catch (const std::exception& e) {
        getLogger()->error("Exception while getting metadata count: {}", e.what());
    }

    getLogger()->info("Metadata storage initialized with {} items", metadataCount);
    return true;
}
void DHTCrawler::performRandomLookup() {
    // Generate a random node ID
    dht::NodeID targetID = dht::generateRandomNodeID();
    // Create a lookup request
    LookupRequest request;
    request.targetID = targetID;
    request.requestTime = std::chrono::steady_clock::now();
    // Add the lookup request to the queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lookupQueue.push(request);
    }
    // Update the lookup rate
    updateLookupRate();
}
void DHTCrawler::handleLookupCompletion(const dht::NodeID& targetID, const std::vector<std::shared_ptr<dht::Node>>& nodes) {
    // Find the lookup request
    std::string targetIDStr = infoHashToString(targetID);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activeLookups.find(targetIDStr);
    if (it == m_activeLookups.end()) {
        return;
    }
    // Mark the lookup as completed
    it->second.completed = true;
    it->second.endTime = std::chrono::steady_clock::now();
    // Remove from active lookups
    m_activeLookups.erase(it);
    // Log the result
    getLogger()->debug("Lookup completed for target ID: {}, found {} nodes", targetIDStr, nodes.size());
}
void DHTCrawler::fetchMetadata(const dht::InfoHash& infoHash) {
    try {
        // Validate the info hash
        if (infoHash.empty()) {
            getLogger()->error("Empty info hash in fetchMetadata");
            return;
        }

        // Make a deep copy of the info hash to ensure it remains valid
        dht::InfoHash infoHashCopy = infoHash;

        // Convert to string for validation and logging
        std::string infoHashStr;
        try {
            infoHashStr = infoHashToString(infoHashCopy);
            if (infoHashStr.empty()) {
                getLogger()->error("Failed to convert info hash to string in fetchMetadata");
                return;
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception converting info hash to string in fetchMetadata: {}", e.what());
            return;
        }

        // Create a metadata fetch request
        MetadataFetchRequest request;
        request.infoHash = infoHashCopy; // Use the copy
        request.requestTime = std::chrono::steady_clock::now();
        request.priority = 0; // Default priority

        // Add the metadata fetch request to the queue
        {
            std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
            try {
                lock.lock();
            } catch (const std::exception& e) {
                getLogger()->error("Exception locking mutex in fetchMetadata: {}", e.what());
                return;
            }

            try {
                m_metadataFetchQueue.push(request);
                m_infoHashesQueued++;
            } catch (const std::exception& e) {
                getLogger()->error("Exception adding to metadata fetch queue: {}", e.what());
                return;
            }
        }

        // Update the metadata fetch rate
        try {
            updateMetadataFetchRate();
        } catch (const std::exception& e) {
            getLogger()->error("Exception updating metadata fetch rate: {}", e.what());
        }

        getLogger()->debug("Queued metadata fetch for info hash: {}", infoHashStr);
    } catch (const std::exception& e) {
        getLogger()->error("Exception in fetchMetadata: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception in fetchMetadata");
    }
}
void DHTCrawler::handleMetadataFetchCompletion(
    const dht::InfoHash& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    bool success) {
    try {
        // Validate input parameters
        if (infoHash.empty()) {
            getLogger()->error("Invalid info hash in handleMetadataFetchCompletion");
            return;
        }

        // Make a deep copy of the info hash and metadata to ensure we're not accessing freed memory
        dht::InfoHash infoHashCopy = infoHash;
        std::string infoHashStr;
        try {
            infoHashStr = infoHashToString(infoHashCopy);
        } catch (const std::exception& e) {
            getLogger()->error("Exception converting info hash to string: {}", e.what());
            return;
        }

        // Only make a copy of metadata if it's valid and we need it
        std::vector<uint8_t> metadataCopy;
        if (success && metadata.data() != nullptr && !metadata.empty() && size > 0 && size <= metadata.size()) {
            try {
                metadataCopy.reserve(metadata.size()); // Pre-allocate memory
                metadataCopy = metadata; // Deep copy
            } catch (const std::exception& e) {
                getLogger()->error("Exception copying metadata: {}", e.what());
                success = false; // Mark as failed since we couldn't copy the metadata
            }
        } else if (success) {
            getLogger()->warning("Invalid metadata received: empty={}, size={}, vector size={}",
                          metadata.empty(), size, metadata.size());
            success = false; // Mark as failed since the metadata is invalid
        }

        // Lock the mutex to access shared data
        std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
        try {
            lock.lock();
        } catch (const std::exception& e) {
            getLogger()->error("Exception locking mutex: {}", e.what());
            return;
        }

        // Find the metadata fetch request
        auto it = m_activeMetadataFetches.find(infoHashStr);
        if (it == m_activeMetadataFetches.end()) {
            getLogger()->warning("Metadata fetch completion for unknown info hash: {}", infoHashStr);
            lock.unlock();
            return;
        }

        // Mark the metadata fetch as completed
        it->second.completed = true;
        it->second.endTime = std::chrono::steady_clock::now();

        // Remove from active metadata fetches
        m_activeMetadataFetches.erase(it);

        // Unlock the mutex before potentially long operations
        lock.unlock();

        // Log the result
        if (success) {
            getLogger()->debug("Metadata fetch completed for info hash: {}, size: {}", infoHashStr, size);
            m_metadataFetched++;

            // Store the metadata if metadata storage is available and metadata is valid
            if (m_metadataStorageAvailable && m_metadataStorage && !metadataCopy.empty()) {
                try {
                    getLogger()->debug("Attempting to store metadata for info hash: {}, size: {}, vector size: {}",
                                 infoHashStr, size, metadataCopy.size());

                    // Use the copied metadata to ensure we're not accessing freed memory
                    if (m_metadataStorage->addMetadata(infoHashCopy, metadataCopy.data(), size)) {
                        getLogger()->info("Stored metadata for info hash: {}", infoHashStr);
                    } else {
                        getLogger()->warning("Failed to store metadata for info hash: {}", infoHashStr);
                    }
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while storing metadata: {}", e.what());
                } catch (...) {
                    getLogger()->error("Unknown exception while storing metadata for info hash: {}", infoHashStr);
                }
            } else {
                if (!m_metadataStorageAvailable || !m_metadataStorage) {
                    getLogger()->debug("Metadata storage not available, skipping storage for info hash: {}", infoHashStr);
                } else {
                    getLogger()->warning("Invalid metadata received for info hash: {}, size: {}, vector size: {}",
                                  infoHashStr, size, metadataCopy.size());
                }
            }
        } else {
            getLogger()->debug("Metadata fetch failed for info hash: {}", infoHashStr);
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception in handleMetadataFetchCompletion: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception in handleMetadataFetchCompletion");
    }
}
void DHTCrawler::handleNewInfoHash(const dht::InfoHash& infoHash) {
    try {
        // Validate the info hash
        if (infoHash.empty()) {
            getLogger()->error("Empty info hash in handleNewInfoHash");
            return;
        }

        // Convert to string with validation
        std::string infoHashStr = infoHashToString(infoHash);
        if (infoHashStr.empty()) {
            getLogger()->error("Failed to convert info hash to string");
            return;
        }

        // Make a deep copy of the info hash to ensure it remains valid
        dht::InfoHash infoHashCopy = infoHash;

        // Check if we've already processed this info hash
        bool shouldFetch = false;
        size_t totalDiscovered = 0;
        {
            std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
            try {
                lock.lock();
            } catch (const std::exception& e) {
                getLogger()->error("Exception locking mutex: {}", e.what());
                return;
            }

            if (m_processedInfoHashes.find(infoHashStr) != m_processedInfoHashes.end()) {
                return;
            }

            // Add to processed info hashes
            try {
                m_processedInfoHashes.insert(infoHashStr);
                m_infoHashesDiscovered++;
                totalDiscovered = m_infoHashesDiscovered;
                shouldFetch = true;
            } catch (const std::exception& e) {
                getLogger()->error("Exception adding to processed info hashes: {}", e.what());
                return;
            }
        }

        // Queue for metadata fetching if needed
        if (shouldFetch) {
            try {
                fetchMetadata(infoHashCopy);
            } catch (const std::exception& e) {
                getLogger()->error("Exception in fetchMetadata: {}", e.what());
            }

            // Log the discovery
            if (totalDiscovered % 10 == 1) { // Log every 10th discovery at INFO level
                getLogger()->info("New info hash discovered: {} (total: {})", infoHashStr, totalDiscovered);
            } else {
                getLogger()->debug("New info hash discovered: {}", infoHashStr);
            }
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception in handleNewInfoHash: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception in handleNewInfoHash");
    }
}
void DHTCrawler::lookupThread() {
    getLogger()->info("Lookup thread started");

    // Counter to alternate between node lookups and info hash lookups
    int lookupCounter = 0;

    while (m_running) {
        // Check if we can perform a lookup
        if (isLookupAllowed()) {
            // Check if we have any lookup requests in the queue
            LookupRequest request;
            bool hasRequest = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_lookupQueue.empty()) {
                    request = m_lookupQueue.front();
                    m_lookupQueue.pop();
                    hasRequest = true;
                }
            }

            if (hasRequest) {
                // Start the lookup
                request.startTime = std::chrono::steady_clock::now();
                // Add to active lookups
                std::string targetIDStr = infoHashToString(request.targetID);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_activeLookups[targetIDStr] = request;
                }

                // Alternate between node lookups and info hash lookups
                if (lookupCounter % 2 == 0) {
                    // Perform a regular node lookup
                    m_dhtNode->findClosestNodes(request.targetID,
                        [this, targetID = request.targetID](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
                            handleLookupCompletion(targetID, nodes);
                        });
                    getLogger()->debug("Started node lookup for target ID: {}", targetIDStr);
                } else {
                    // Perform a get_peers lookup to discover info hashes
                    m_dhtNode->findPeers(request.targetID,
                        [this, targetID = request.targetID](const std::vector<network::EndPoint>& peers,
                                                          const std::vector<std::shared_ptr<dht::Node>>& nodes,
                                                          const std::string& /* token */) {
                            // We don't need to do anything with the results here
                            // The DHT node will forward discovered info hashes to the collector
                            getLogger()->debug("Completed get_peers lookup for info hash: {}, found {} peers and {} nodes",
                                         infoHashToString(targetID), peers.size(), nodes.size());
                            handleLookupCompletion(targetID, nodes);
                        });
                    getLogger()->debug("Started get_peers lookup for info hash: {}", targetIDStr);
                }

                lookupCounter++;
            } else {
                // No lookup requests in the queue, generate a random one
                performRandomLookup();
            }
        }
        // Sleep for the lookup interval
        std::this_thread::sleep_for(m_config.lookupInterval);
    }
    getLogger()->debug("Lookup thread stopped");
}
void DHTCrawler::metadataFetchThread() {
    getLogger()->info("Metadata fetch thread started");
    while (m_running) {
        try {
            // Check if we can perform a metadata fetch
            if (isMetadataFetchAllowed()) {
                // Check if we have any metadata fetch requests in the queue
                MetadataFetchRequest request;
                bool hasRequest = false;
                {
                    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
                    try {
                        lock.lock();
                    } catch (const std::exception& e) {
                        getLogger()->error("Exception locking mutex: {}", e.what());
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    if (!m_metadataFetchQueue.empty()) {
                        try {
                            request = m_metadataFetchQueue.top();
                            m_metadataFetchQueue.pop();
                            hasRequest = true;
                        } catch (const std::exception& e) {
                            getLogger()->error("Exception accessing metadata fetch queue: {}", e.what());
                            hasRequest = false;
                        }
                    }
                }
                if (hasRequest) {
                    // Validate the info hash
                    if (request.infoHash.empty()) {
                        getLogger()->warning("Invalid info hash in metadata fetch request");
                        continue;
                    }

                    // Start the metadata fetch
                    request.startTime = std::chrono::steady_clock::now();

                    // Convert info hash to string for logging and map key
                    std::string infoHashStr;
                    try {
                        infoHashStr = infoHashToString(request.infoHash);
                    } catch (const std::exception& e) {
                        getLogger()->error("Exception converting info hash to string: {}", e.what());
                        continue;
                    }

                    // Add to active metadata fetches
                    {
                        std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
                        try {
                            lock.lock();
                            m_activeMetadataFetches[infoHashStr] = request;
                        } catch (const std::exception& e) {
                            getLogger()->error("Exception adding to active metadata fetches: {}", e.what());
                            continue;
                        }
                    }

                    // Perform the metadata fetch
                    try {
                        std::vector<network::EndPoint> endpoints; // Empty endpoints, will use DHT

                        // Create a copy of the info hash to ensure it remains valid
                        dht::InfoHash infoHashCopy = request.infoHash;

                        // Use a shared_ptr to ensure the callback remains valid even if this object is destroyed
                        auto callback = [this, infoHashCopy](const std::array<uint8_t, 20>& /* infoHash */,
                                                          const std::vector<uint8_t>& metadata,
                                                          uint32_t size,
                                                          bool success) {
                            try {
                                // Validate parameters
                                if (infoHashCopy.empty()) {
                                    getLogger()->error("Invalid info hash in metadata fetch callback");
                                    return;
                                }

                                // Check if metadata is valid when success is true
                                if (success && (metadata.empty() || size == 0 || size > metadata.size())) {
                                    getLogger()->warning("Invalid metadata in callback: empty={}, size={}, vector size={}",
                                                  metadata.empty(), size, metadata.size());
                                    success = false; // Mark as failed since the metadata is invalid
                                }

                                handleMetadataFetchCompletion(infoHashCopy, metadata, size, success);
                            } catch (const std::exception& e) {
                                getLogger()->error("Exception in metadata fetch callback: {}", e.what());
                            } catch (...) {
                                getLogger()->error("Unknown exception in metadata fetch callback");
                            }
                        };

                        if (m_metadataFetcher && m_metadataFetcher->isRunning()) {
                            m_metadataFetcher->fetchMetadata(infoHashCopy, endpoints, callback);
                            getLogger()->debug("Started metadata fetch for info hash: {}", infoHashStr);
                        } else {
                            getLogger()->warning("Metadata fetcher not available or not running");

                            // Remove from active metadata fetches since we couldn't start the fetch
                            std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
                            try {
                                lock.lock();
                                m_activeMetadataFetches.erase(infoHashStr);
                            } catch (const std::exception& e) {
                                getLogger()->error("Exception removing from active metadata fetches: {}", e.what());
                            }
                        }
                    } catch (const std::exception& e) {
                        getLogger()->error("Exception while starting metadata fetch: {}", e.what());

                        // Remove from active metadata fetches since we couldn't start the fetch
                        std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
                        try {
                            lock.lock();
                            m_activeMetadataFetches.erase(infoHashStr);
                        } catch (const std::exception& e) {
                            getLogger()->error("Exception removing from active metadata fetches: {}", e.what());
                        }
                    } catch (...) {
                        getLogger()->error("Unknown exception while starting metadata fetch");

                        // Remove from active metadata fetches since we couldn't start the fetch
                        std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
                        try {
                            lock.lock();
                            m_activeMetadataFetches.erase(infoHashStr);
                        } catch (const std::exception& e) {
                            getLogger()->error("Exception removing from active metadata fetches: {}", e.what());
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception in metadata fetch thread: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception in metadata fetch thread");
        }

        // Sleep for the metadata fetch interval
        std::this_thread::sleep_for(m_config.metadataFetchInterval);
    }
    getLogger()->debug("Metadata fetch thread stopped");
}
void DHTCrawler::updateWindowTitle() {
    // Update statistics
    // Only get the statistics we need for the title
    uint64_t metadataFetched = m_metadataFetched;
    double lookupRate = m_lookupRate;

    // Get the total number of info hashes from the collector
    uint64_t totalInfoHashes = 0;
    if (m_infoHashCollector) {
        totalInfoHashes = m_infoHashCollector->getInfoHashCount();
    }

    // Get the routing table size
    size_t routingTableSize = 0;
    if (m_dhtNode) {
        routingTableSize = m_dhtNode->getRoutingTable().getNodeCount();
    }

    // Get memory usage
    uint64_t memoryUsage = util::ProcessUtils::getMemoryUsage();
    std::string memoryUsageStr = util::ProcessUtils::formatSize(memoryUsage);

    // Update terminal window title with stats
    std::string title = util::FilesystemUtils::getExecutableName() +
                      " - InfoHashes: " + std::to_string(totalInfoHashes) +
                      " - Metadata: " + std::to_string(metadataFetched) +
                      " - RT: " + std::to_string(routingTableSize) +
                      " - Mem: " + memoryUsageStr +
                      " - Rate: " + std::to_string(static_cast<int>(lookupRate)) + "/min";
    util::FilesystemUtils::setTerminalTitle(title);
}

void DHTCrawler::statusThread() {
    getLogger()->info("Status thread started");

    // Time tracking for status updates and title updates
    auto lastStatusTime = std::chrono::steady_clock::now();
    auto lastTitleTime = lastStatusTime;

    while (m_running) {
        auto now = std::chrono::steady_clock::now();

        // Update the window title every 3 seconds
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTitleTime).count() >= 3) {
            updateWindowTitle();
            lastTitleTime = now;
        }

        // Full status update at the configured interval
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusTime).count() >=
            std::chrono::duration_cast<std::chrono::seconds>(m_config.statusInterval).count()) {

            // Update statistics
            uint64_t infoHashesDiscovered = m_infoHashesDiscovered;
            uint64_t infoHashesQueued = m_infoHashesQueued;
            uint64_t metadataFetched = m_metadataFetched;
            double lookupRate = m_lookupRate;
            double metadataFetchRate = m_metadataFetchRate;

            // Get the total number of info hashes from the collector
            uint64_t totalInfoHashes = 0;
            if (m_infoHashCollector) {
                totalInfoHashes = m_infoHashCollector->getInfoHashCount();
            }

            // Get the routing table size
            size_t routingTableSize = 0;
            if (m_dhtNode) {
                routingTableSize = m_dhtNode->getRoutingTable().getNodeCount();
            }

            // Get memory usage
            uint64_t memoryUsage = util::ProcessUtils::getMemoryUsage();
            std::string memoryUsageStr = util::ProcessUtils::formatSize(memoryUsage);

            // Log status with total info hashes
            getLogger()->info("Status: {} info hashes discovered (total: {}), {} queued, {} metadata fetched, routing table: {} nodes, memory: {}, lookup rate: {:.2f}/min, metadata fetch rate: {:.2f}/min",
                        infoHashesDiscovered, totalInfoHashes, infoHashesQueued, metadataFetched, routingTableSize, memoryUsageStr, lookupRate, metadataFetchRate);

            // Call the status callback
            if (m_statusCallback) {
                m_statusCallback(infoHashesDiscovered, infoHashesQueued, metadataFetched, lookupRate, metadataFetchRate, totalInfoHashes, routingTableSize, memoryUsage);
            }

            // Prioritize info hashes
            if (m_config.prioritizePopularInfoHashes) {
                prioritizeInfoHashes();
            }

            lastStatusTime = now;
        }

        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    getLogger()->debug("Status thread stopped");
}

void DHTCrawler::activeNodeDiscoveryThread() {
    getLogger()->info("Active node discovery thread started");

    // Wait a bit before starting to allow the DHT node to initialize
    std::this_thread::sleep_for(std::chrono::seconds(5));

    while (m_running) {
        try {
            // Perform random node lookups to discover more nodes
            if (m_dhtNode && m_dhtNode->isRunning()) {
                getLogger()->debug("Performing random node lookup for active discovery");
                m_dhtNode->performRandomNodeLookup();
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception in active node discovery thread: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception in active node discovery thread");
        }

        // Sleep for the active node discovery interval
        std::this_thread::sleep_for(std::chrono::seconds(m_config.activeNodeDiscoveryInterval));
    }

    getLogger()->debug("Active node discovery thread stopped");
}
void DHTCrawler::updateLookupRate() {
    auto now = std::chrono::steady_clock::now();
    // Add the current time to the lookup times
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lookupTimes.push_back(now);
        // Remove lookup times older than 1 minute
        auto oneMinuteAgo = now - std::chrono::minutes(1);
        m_lookupTimes.erase(
            std::remove_if(m_lookupTimes.begin(), m_lookupTimes.end(),
                          [oneMinuteAgo](const auto& time) { return time < oneMinuteAgo; }),
            m_lookupTimes.end());
        // Calculate the lookup rate
        m_lookupRate = static_cast<double>(m_lookupTimes.size());
    }
}
void DHTCrawler::updateMetadataFetchRate() {
    try {
        auto now = std::chrono::steady_clock::now();
        // Add the current time to the metadata fetch times
        {
            std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
            try {
                lock.lock();
            } catch (const std::exception& e) {
                getLogger()->error("Exception locking mutex in updateMetadataFetchRate: {}", e.what());
                return;
            }

            try {
                // Add the current time to the metadata fetch times
                m_metadataFetchTimes.push_back(now);

                // Remove metadata fetch times older than 1 minute
                auto oneMinuteAgo = now - std::chrono::minutes(1);

                // Use a safer approach to remove elements
                std::vector<std::chrono::steady_clock::time_point> newTimes;
                newTimes.reserve(m_metadataFetchTimes.size()); // Pre-allocate memory

                for (const auto& time : m_metadataFetchTimes) {
                    if (time >= oneMinuteAgo) {
                        newTimes.push_back(time);
                    }
                }

                m_metadataFetchTimes = std::move(newTimes);

                // Calculate the metadata fetch rate
                m_metadataFetchRate = static_cast<double>(m_metadataFetchTimes.size());
            } catch (const std::exception& e) {
                getLogger()->error("Exception updating metadata fetch rate: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception in updateMetadataFetchRate: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception in updateMetadataFetchRate");
    }
}
bool DHTCrawler::isLookupAllowed() {
    // Check if we have reached the maximum number of concurrent lookups
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeLookups.size() >= m_config.maxConcurrentLookups) {
            return false;
        }
    }
    // Check if we have reached the maximum lookup rate
    if (m_lookupRate >= m_config.maxLookupsPerMinute) {
        return false;
    }
    return true;
}
bool DHTCrawler::isMetadataFetchAllowed() {
    // Check if we have reached the maximum number of concurrent metadata fetches
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeMetadataFetches.size() >= m_config.maxConcurrentMetadataFetches) {
            return false;
        }
    }
    // Check if we have reached the maximum metadata fetch rate
    if (m_metadataFetchRate >= m_config.maxMetadataFetchesPerMinute) {
        return false;
    }
    return true;
}
void DHTCrawler::prioritizeInfoHashes() {
    // This is a simple implementation that prioritizes info hashes based on their age
    // More sophisticated prioritization could be implemented based on popularity, etc.
    std::lock_guard<std::mutex> lock(m_mutex);
    // Create a temporary queue
    std::priority_queue<MetadataFetchRequest> tempQueue;
    // Process all requests in the queue
    while (!m_metadataFetchQueue.empty()) {
        auto request = m_metadataFetchQueue.top();
        m_metadataFetchQueue.pop();
        // Calculate priority based on age (older requests get higher priority)
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - request.requestTime).count();
        request.priority = static_cast<uint32_t>(age);
        // Add to the temporary queue
        tempQueue.push(request);
    }
    // Swap the queues
    m_metadataFetchQueue = std::move(tempQueue);
}
std::string DHTCrawler::infoHashToString(const dht::InfoHash& infoHash) const {
    try {
        // Validate the info hash
        if (infoHash.empty()) {
            getLogger()->error("Empty info hash in infoHashToString");
            return "";
        }

        // Check if the info hash data pointer is valid
        if (infoHash.data() == nullptr) {
            getLogger()->error("Null data pointer in info hash");
            return "";
        }

        // Check if the info hash size is valid
        if (infoHash.size() != 20) {
            getLogger()->error("Invalid info hash size: {}", infoHash.size());
            return "";
        }

        // Convert to hex string
        return util::bytesToHex(infoHash.data(), infoHash.size());
    } catch (const std::exception& e) {
        getLogger()->error("Exception in infoHashToString: {}", e.what());
        return "";
    } catch (...) {
        getLogger()->error("Unknown exception in infoHashToString");
        return "";
    }
}
} // namespace dht_hunter::crawler
