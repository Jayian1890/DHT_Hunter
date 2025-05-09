#include "dht_hunter/dht/crawler/crawler.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include "dht_hunter/unified_event/events/peer_events.hpp"
#include "dht_hunter/unified_event/events/message_events.hpp"
#include "dht_hunter/unified_event/events/custom_events.hpp"
#include <algorithm>
#include <random>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<Crawler> Crawler::s_instance = nullptr;
std::mutex Crawler::s_instanceMutex;

std::shared_ptr<Crawler> Crawler::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingManager> routingManager,
    std::shared_ptr<NodeLookup> nodeLookup,
    std::shared_ptr<PeerLookup> peerLookup,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<PeerStorage> peerStorage,
    const CrawlerConfig& crawlerConfig) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<Crawler>(new Crawler(
                    config, nodeID, routingManager, nodeLookup, peerLookup,
                    transactionManager, messageSender, peerStorage, crawlerConfig));
            }
            return s_instance;
        }, "Crawler::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return nullptr;
    }
}

Crawler::Crawler(const DHTConfig& config,
                 const NodeID& nodeID,
                 std::shared_ptr<RoutingManager> routingManager,
                 std::shared_ptr<NodeLookup> nodeLookup,
                 std::shared_ptr<PeerLookup> peerLookup,
                 std::shared_ptr<TransactionManager> transactionManager,
                 std::shared_ptr<MessageSender> messageSender,
                 std::shared_ptr<PeerStorage> peerStorage,
                 const CrawlerConfig& crawlerConfig)
    : m_config(config),
      m_nodeID(nodeID),
      m_crawlerConfig(crawlerConfig),
      m_routingManager(routingManager),
      m_nodeLookup(nodeLookup),
      m_peerLookup(peerLookup),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_peerStorage(peerStorage),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_running(false) {

    // Subscribe to events
    if (m_eventBus) {
        // Subscribe to node discovered events
        int nodeDiscoveredId = m_eventBus->subscribe(
            unified_event::EventType::NodeDiscovered,
            [this](const std::shared_ptr<unified_event::Event>& event) {
                this->handleNodeDiscoveredEvent(event);
            });
        m_eventSubscriptionIds.push_back(nodeDiscoveredId);

        // Subscribe to peer discovered events
        int peerDiscoveredId = m_eventBus->subscribe(
            unified_event::EventType::PeerDiscovered,
            [this](const std::shared_ptr<unified_event::Event>& event) {
                this->handlePeerDiscoveredEvent(event);
            });
        m_eventSubscriptionIds.push_back(peerDiscoveredId);

        // Subscribe to message received events
        int messageReceivedId = m_eventBus->subscribe(
            unified_event::EventType::MessageReceived,
            [this](const std::shared_ptr<unified_event::Event>& event) {
                this->handleMessageReceivedEvent(event);
            });
        m_eventSubscriptionIds.push_back(messageReceivedId);

        // Subscribe to message sent events
        int messageSentId = m_eventBus->subscribe(
            unified_event::EventType::MessageSent,
            [this](const std::shared_ptr<unified_event::Event>& event) {
                this->handleMessageSentEvent(event);
            });
        m_eventSubscriptionIds.push_back(messageSentId);
    }

    // Auto-start if configured
    if (m_crawlerConfig.autoStart) {
        start();
        unified_event::logDebug("DHT.Crawler", "Crawler started automatically");
    }
}

Crawler::~Crawler() {
    // Stop the crawler
    stop();

    // Unsubscribe from events
    if (m_eventBus) {
        for (int subscriptionId : m_eventSubscriptionIds) {
            m_eventBus->unsubscribe(subscriptionId);
        }
    }

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "Crawler::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

bool Crawler::start() {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            if (m_running) {
                return true;
            }

            m_running = true;
            m_crawlThread = std::thread(&Crawler::crawlThread, this);

            return m_running;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return false;
    }
}

void Crawler::stop() {
    try {
        bool shouldJoin = utility::thread::withLock(m_mutex, [this]() {
            if (!m_running) {
                return false;
            }

            unified_event::logInfo("DHT.Crawler", "Stopping crawler");

            m_running = false;
            return true;
        }, "Crawler::m_mutex");

        // Only join the thread if we actually stopped the crawler
        if (!shouldJoin) {
            return;
        }
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return;
    }

    if (m_crawlThread.joinable()) {
        m_crawlThread.join();
    }
}

bool Crawler::isRunning() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_running;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return false;
    }
}

CrawlerStatistics Crawler::getStatistics() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_statistics;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return CrawlerStatistics();
    }
}

std::vector<std::shared_ptr<Node>> Crawler::getDiscoveredNodes(size_t maxNodes) const {
    try {
        return utility::thread::withLock(m_mutex, [this, maxNodes]() {
            std::vector<std::shared_ptr<Node>> nodes;
            nodes.reserve(m_discoveredNodes.size());

            for (const auto& [_, node] : m_discoveredNodes) {
                nodes.push_back(node);
                if (maxNodes > 0 && nodes.size() >= maxNodes) {
                    break;
                }
            }

            return nodes;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return std::vector<std::shared_ptr<Node>>();
    }
}

std::vector<InfoHash> Crawler::getDiscoveredInfoHashes(size_t maxInfoHashes) const {
    try {
        return utility::thread::withLock(m_mutex, [this, maxInfoHashes]() {
            std::vector<InfoHash> infoHashes;
            infoHashes.reserve(m_discoveredInfoHashes.size());

            for (const auto& [_, infoHash] : m_discoveredInfoHashes) {
                infoHashes.push_back(infoHash);
                if (maxInfoHashes > 0 && infoHashes.size() >= maxInfoHashes) {
                    break;
                }
            }

            return infoHashes;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return std::vector<InfoHash>();
    }
}

std::vector<EndPoint> Crawler::getPeersForInfoHash(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            std::vector<EndPoint> peers;
            std::string infoHashStr = infoHashToString(infoHash);

            auto it = m_infoHashPeers.find(infoHashStr);
            if (it != m_infoHashPeers.end()) {
                for (const auto& peerStr : it->second) {
                    // Parse the peer string (format: "ip:port")
                    size_t colonPos = peerStr.find(':');
                    if (colonPos != std::string::npos) {
                        std::string ip = peerStr.substr(0, colonPos);
                        uint16_t port = static_cast<uint16_t>(std::stoi(peerStr.substr(colonPos + 1)));
                        network::NetworkAddress address(ip);
                        EndPoint endpoint(address, port);
                        peers.push_back(endpoint);
                    }
                }
            }

            return peers;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return std::vector<EndPoint>();
    }
}

void Crawler::monitorInfoHash(const InfoHash& infoHash) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash]() {
            m_monitoredInfoHashes.insert(infoHashToString(infoHash));
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

void Crawler::stopMonitoringInfoHash(const InfoHash& infoHash) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash]() {
            m_monitoredInfoHashes.erase(infoHashToString(infoHash));
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

bool Crawler::isMonitoringInfoHash(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            return m_monitoredInfoHashes.find(infoHashToString(infoHash)) != m_monitoredInfoHashes.end();
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return false;
    }
}

std::vector<InfoHash> Crawler::getMonitoredInfoHashes() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            std::vector<InfoHash> infoHashes;
            infoHashes.reserve(m_monitoredInfoHashes.size());

            for (const auto& infoHashStr : m_monitoredInfoHashes) {
                InfoHash infoHash;
                if (infoHashFromString(infoHashStr, infoHash)) {
                    infoHashes.push_back(infoHash);
                }
            }

            return infoHashes;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
        return std::vector<InfoHash>();
    }
}

void Crawler::crawlThread() {
    while (m_running) {
        try {
            // Perform a crawl iteration
            crawl();

            // Sleep for the refresh interval
            std::this_thread::sleep_for(std::chrono::seconds(m_crawlerConfig.refreshInterval));
        } catch (const std::exception& e) {
            unified_event::logError("DHT.Crawler", "Exception in crawl thread: " + std::string(e.what()));
        } catch (...) {
            unified_event::logError("DHT.Crawler", "Unknown exception in crawl thread");
        }
    }
}

void Crawler::crawl() {
    // Log the start of a crawl iteration
    unified_event::logDebug("DHT.Crawler", "Starting crawl iteration");

    // Discover new nodes
    discoverNodes();

    // Monitor info hashes
    monitorInfoHashes();

    // Update statistics
    updateStatistics();

    // Log the completion of a crawl iteration with statistics
    std::string statsMessage = "Completed crawl iteration - ";
    statsMessage += "Nodes: " + std::to_string(m_statistics.nodesDiscovered) + ", ";
    statsMessage += "Info hashes: " + std::to_string(m_statistics.infoHashesDiscovered) + ", ";
    statsMessage += "Peers: " + std::to_string(m_statistics.peersDiscovered);
    unified_event::logDebug("DHT.Crawler", statsMessage);
}

void Crawler::discoverNodes() {
    // Get a random subset of nodes to crawl
    std::vector<std::shared_ptr<Node>> nodesToCrawl;
    try {
        nodesToCrawl = utility::thread::withLock(m_mutex, [this]() {
            // Get all nodes
            std::vector<std::shared_ptr<Node>> allNodes;
            allNodes.reserve(m_discoveredNodes.size());
            for (const auto& [_, node] : m_discoveredNodes) {
                allNodes.push_back(node);
            }

            // If we don't have enough nodes, use the routing table
            if (allNodes.size() < m_crawlerConfig.parallelCrawls && m_routingManager) {
                auto routingTableNodes = m_routingManager->getRoutingTable()->getAllNodes();
                allNodes.insert(allNodes.end(), routingTableNodes.begin(), routingTableNodes.end());
            }

            // Shuffle the nodes
            std::random_device rd;
            std::mt19937 g(rd());
            std::ranges::shuffle(allNodes, g);

            // Take the first N nodes
            const size_t nodesToTake = std::min(m_crawlerConfig.parallelCrawls, allNodes.size());
            std::vector<std::shared_ptr<Node>> result;
            result.assign(allNodes.begin(), allNodes.begin() + static_cast<long>(nodesToTake));
            return result;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }

    // Log node selection information
    std::string selectionMessage = "Node selection - Selected for crawl: " + std::to_string(nodesToCrawl.size());
    unified_event::logDebug("DHT.Crawler", selectionMessage);

    // Perform node lookups
    for (const auto& node : nodesToCrawl) {
        if (m_nodeLookup) {
            std::string nodeIDStr = nodeIDToString(node->getID());
            std::string endpoint = node->getEndpoint().toString();

            std::string lookupMessage = "Looking up nodes near " + nodeIDStr + " at " + endpoint;
            unified_event::logDebug("DHT.Crawler", lookupMessage);

            m_nodeLookup->lookup(node->getID(), [this, nodeIDStr](const std::vector<std::shared_ptr<Node>>& nodes) {
                try {
                    size_t newNodes = utility::thread::withLock(m_mutex, [this, &nodes]() {
                        size_t newNodesCount = 0;

                        // Add the nodes to our discovered nodes
                        for (const auto& node : nodes) {
                            std::string discoveredNodeIDStr = nodeIDToString(node->getID());
                            bool isNew = m_discoveredNodes.find(discoveredNodeIDStr) == m_discoveredNodes.end();

                            m_discoveredNodes[discoveredNodeIDStr] = node;

                            if (isNew) {
                                newNodesCount++;
                            }
                        }

                        // Update statistics
                        m_statistics.nodesDiscovered = m_discoveredNodes.size();

                        return newNodesCount;
                    }, "Crawler::m_mutex");

                    std::string completionMessage = "Node lookup for " + nodeIDStr + " completed";
                    completionMessage += " - Found: " + std::to_string(nodes.size());
                    completionMessage += ", New: " + std::to_string(newNodes);
                    unified_event::logDebug("DHT.Crawler", completionMessage);
                } catch (const utility::thread::LockTimeoutException& e) {
                    unified_event::logError("DHT.Crawler", e.what());
                }
            });
        }
    }

    // Generate some random node IDs to look up
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string generatingMessage = "Generating " + std::to_string(m_crawlerConfig.parallelCrawls) + " random node IDs for lookup";
    unified_event::logDebug("DHT.Crawler", generatingMessage);

    for (size_t i = 0; i < m_crawlerConfig.parallelCrawls; ++i) {
        // Generate a random node ID
        std::vector<uint8_t> randomNodeIDVec(20);
        for (size_t j = 0; j < 20; ++j) {
            randomNodeIDVec[j] = static_cast<uint8_t>(dis(g));
        }

        // Create a NodeID from the vector
        std::array<uint8_t, 20> randomNodeIDArray;
        std::copy_n(randomNodeIDVec.begin(), 20, randomNodeIDArray.begin());
        NodeID randomNodeID(randomNodeIDArray);

        std::string randomNodeIDStr = nodeIDToString(randomNodeID);
        std::string randomLookupMessage = "Looking up random node ID: " + randomNodeIDStr;
        unified_event::logDebug("DHT.Crawler", randomLookupMessage);

        // Perform a node lookup
        if (m_nodeLookup) {
            m_nodeLookup->lookup(randomNodeID, [this, randomNodeIDStr](const std::vector<std::shared_ptr<Node>>& nodes) {
                try {
                    size_t newNodes = utility::thread::withLock(m_mutex, [this, &nodes]() {
                        size_t newNodesCount = 0;

                        // Add the nodes to our discovered nodes
                        for (const auto& node : nodes) {
                            std::string discoveredNodeIDStr = nodeIDToString(node->getID());
                            bool isNew = m_discoveredNodes.find(discoveredNodeIDStr) == m_discoveredNodes.end();

                            m_discoveredNodes[discoveredNodeIDStr] = node;

                            if (isNew) {
                                newNodesCount++;
                            }
                        }

                        // Update statistics
                        m_statistics.nodesDiscovered = m_discoveredNodes.size();

                        return newNodesCount;
                    }, "Crawler::m_mutex");

                    std::string randomCompletionMessage = "Random node lookup for " + randomNodeIDStr + " completed";
                    randomCompletionMessage += " - Found: " + std::to_string(nodes.size());
                    randomCompletionMessage += ", New: " + std::to_string(newNodes);
                    unified_event::logDebug("DHT.Crawler", randomCompletionMessage);
                } catch (const utility::thread::LockTimeoutException& e) {
                    unified_event::logError("DHT.Crawler", e.what());
                }
            });
        }
    }
}

void Crawler::monitorInfoHashes() {
    unified_event::logDebug("DHT.Crawler", "Starting info hash monitoring");

    // Limit the number of info hashes to monitor to avoid hanging
    const size_t MAX_CONCURRENT_LOOKUPS = 5; // Increased from 3 to 5

    std::vector<InfoHash> infoHashesToMonitor;
    try {
        infoHashesToMonitor = utility::thread::withLock(m_mutex, [this]() {
            std::vector<InfoHash> result;

            // Get all monitored info hashes
            for (const auto& infoHashStr : m_monitoredInfoHashes) {
                InfoHash infoHash;
                if (infoHashFromString(infoHashStr, infoHash)) {
                    result.push_back(infoHash);
                }
            }

            return result;
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }

    unified_event::logDebug("DHT.Crawler", "Monitoring " + std::to_string(infoHashesToMonitor.size()) + " info hashes");

    // Limit the number of info hashes to monitor in each iteration
    size_t numToMonitor = std::min(infoHashesToMonitor.size(), MAX_CONCURRENT_LOOKUPS);

    // Perform peer lookups for a limited number of info hashes
    for (size_t i = 0; i < numToMonitor; ++i) {
        const auto& infoHash = infoHashesToMonitor[i];
        if (m_peerLookup) {
            std::string infoHashStr = infoHashToString(infoHash);
            unified_event::logDebug("DHT.Crawler", "Looking up peers for info hash: " + infoHashStr);

            // First lookup to find initial peers
            m_peerLookup->lookup(infoHash, [this, infoHash](const std::vector<network::EndPoint>& peers) {
                try {
                    std::string infoHashStr = infoHashToString(infoHash);
                    size_t totalPeers = utility::thread::withLock(m_mutex, [this, &infoHash, &peers, &infoHashStr]() {
                        // Add the info hash to our discovered info hashes
                        m_discoveredInfoHashes[infoHashStr] = infoHash;

                        // Add the peers to our info hash peers
                        auto& peerSet = m_infoHashPeers[infoHashStr];
                        size_t newPeers = 0;
                        for (const auto& peer : peers) {
                            std::string peerStr = peer.toString();
                            if (peerSet.find(peerStr) == peerSet.end()) {
                                peerSet.insert(peerStr);
                                newPeers++;
                            }
                        }

                        // Update statistics
                        m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
                        m_statistics.peersDiscovered = 0;
                        for (const auto& infoHashPeersPair : m_infoHashPeers) {
                            const auto& peerSetCount = infoHashPeersPair.second;
                            m_statistics.peersDiscovered += peerSetCount.size();
                        }

                        unified_event::logDebug("DHT.Crawler", "Added " + std::to_string(newPeers) +
                                             " new peers for InfoHash: " + infoHashStr +
                                             ", total peers: " + std::to_string(peerSet.size()));

                        return peerSet.size();
                    }, "Crawler::m_mutex");

                    std::string completionMessage = "Peer lookup for info hash " + infoHashStr + " completed";
                    completionMessage += " - Found: " + std::to_string(peers.size());
                    completionMessage += ", Total: " + std::to_string(totalPeers);
                    unified_event::logDebug("DHT.Crawler", completionMessage);

                    // Continue searching for more peers even after finding some
                    // Schedule a second lookup after a short delay to find more peers
                    if (m_running && !peers.empty()) {
                        unified_event::logDebug("DHT.Crawler", "Scheduling additional peer lookup for info hash: " + infoHashStr);

                        // Use a separate thread to avoid blocking
                        std::thread([this, infoHash, infoHashStr]() {
                            // Wait a bit before doing another lookup
                            std::this_thread::sleep_for(std::chrono::seconds(5));

                            if (!m_running) return; // Check if still running

                            // Do another lookup to find more peers
                            if (m_peerLookup) {
                                m_peerLookup->lookup(infoHash, [this, infoHash, infoHashStr](const std::vector<network::EndPoint>& morePeers) {
                                    try {
                                        size_t additionalPeers = utility::thread::withLock(m_mutex, [this, &infoHash, &morePeers, &infoHashStr]() {
                                            // Add the additional peers
                                            auto& peerSet = m_infoHashPeers[infoHashStr];
                                            size_t newPeers = 0;
                                            for (const auto& peer : morePeers) {
                                                std::string peerStr = peer.toString();
                                                if (peerSet.find(peerStr) == peerSet.end()) {
                                                    peerSet.insert(peerStr);
                                                    newPeers++;
                                                }
                                            }

                                            // Update statistics
                                            m_statistics.peersDiscovered = 0;
                                            for (const auto& infoHashPeersPair : m_infoHashPeers) {
                                                const auto& peerSetCount = infoHashPeersPair.second;
                                                m_statistics.peersDiscovered += peerSetCount.size();
                                            }

                                            if (newPeers > 0) {
                                                unified_event::logDebug("DHT.Crawler", "Additional lookup found " +
                                                                     std::to_string(newPeers) +
                                                                     " new peers for InfoHash: " + infoHashStr +
                                                                     ", total peers: " + std::to_string(peerSet.size()));
                                            }

                                            return newPeers;
                                        }, "Crawler::m_mutex");

                                        if (additionalPeers > 0) {
                                            std::string additionalMessage = "Additional peer lookup for info hash " + infoHashStr + " completed";
                                            additionalMessage += " - Found: " + std::to_string(morePeers.size());
                                            additionalMessage += ", New: " + std::to_string(additionalPeers);
                                            unified_event::logDebug("DHT.Crawler", additionalMessage);
                                        }
                                    } catch (const utility::thread::LockTimeoutException& e) {
                                        unified_event::logError("DHT.Crawler", e.what());
                                    }
                                });
                            }
                        }).detach(); // Detach the thread so it runs independently
                    }
                } catch (const utility::thread::LockTimeoutException& e) {
                    unified_event::logError("DHT.Crawler", e.what());
                }
            });
        }
    }

    // Generate some random info hashes to look up
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dis(0, 255);

    // Limit the number of random lookups to avoid hanging
    const size_t MAX_RANDOM_LOOKUPS = 2;
    size_t numRandomLookups = std::min(m_crawlerConfig.parallelCrawls, MAX_RANDOM_LOOKUPS);

    unified_event::logDebug("DHT.Crawler", "Generating " + std::to_string(numRandomLookups) + " random info hashes for lookup");

    for (size_t i = 0; i < numRandomLookups; ++i) {
        // Generate a random info hash
        std::vector<uint8_t> randomInfoHashVec(20);
        for (size_t j = 0; j < 20; ++j) {
            randomInfoHashVec[j] = static_cast<uint8_t>(dis(g));
        }

        // Create an InfoHash from the vector
        std::array<uint8_t, 20> randomInfoHashArray;
        std::copy_n(randomInfoHashVec.begin(), 20, randomInfoHashArray.begin());
        InfoHash randomInfoHash(randomInfoHashArray);

        std::string randomInfoHashStr = infoHashToString(randomInfoHash);
        std::string randomHashLookupMessage = "Looking up random info hash: " + randomInfoHashStr;
        unified_event::logDebug("DHT.Crawler", randomHashLookupMessage);

        // Perform a peer lookup
        if (m_peerLookup) {
            m_peerLookup->lookup(randomInfoHash, [this, randomInfoHash, randomInfoHashStr](const std::vector<network::EndPoint>& peers) {
                try {
                    utility::thread::withLock(m_mutex, [this, &peers, &randomInfoHash, &randomInfoHashStr]() {
                        // Only add the info hash if we found peers
                        if (!peers.empty()) {
                            // Check if this is a new info hash
                            bool isNewInfoHash = m_discoveredInfoHashes.find(randomInfoHashStr) == m_discoveredInfoHashes.end();

                            // Add the info hash to our discovered info hashes
                            m_discoveredInfoHashes[randomInfoHashStr] = randomInfoHash;

                            // Add the peers to our info hash peers
                            auto& peerSet = m_infoHashPeers[randomInfoHashStr];
                            for (const auto& peer : peers) {
                                peerSet.insert(peer.toString());
                            }

                            // Publish an InfoHashDiscoveredEvent if this is a new info hash
                            if (isNewInfoHash && m_eventBus) {
                                auto event = std::make_shared<unified_event::InfoHashDiscoveredEvent>("DHT.Crawler", randomInfoHash);
                                m_eventBus->publish(event);
                            }

                            // Update statistics
                            m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
                            m_statistics.peersDiscovered = 0;
                            for (const auto& infoHashPeersPair : m_infoHashPeers) {
                                const auto& peerSetCount = infoHashPeersPair.second;
                                m_statistics.peersDiscovered += peerSetCount.size();
                            }
                        }
                    }, "Crawler::m_mutex");

                    if (!peers.empty()) {
                        std::string activeTorrentMessage = "Found active torrent - Info hash: " + randomInfoHashStr;
                        activeTorrentMessage += ", Peers: " + std::to_string(peers.size());
                        unified_event::logDebug("DHT.Crawler", activeTorrentMessage);
                    } else {
                        std::string noPeersMessage = "No peers found for random info hash: " + randomInfoHashStr;
                        unified_event::logDebug("DHT.Crawler", noPeersMessage);
                    }
                } catch (const utility::thread::LockTimeoutException& e) {
                    unified_event::logError("DHT.Crawler", e.what());
                }
            });
        }
    }
}

void Crawler::updateStatistics() {
    unified_event::logDebug("DHT.Crawler", "Updating statistics and pruning data");

    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Limit the number of nodes and info hashes stored
            if (m_discoveredNodes.size() > m_crawlerConfig.maxNodes) {
                // Remove the oldest nodes
                size_t nodesToRemove = m_discoveredNodes.size() - m_crawlerConfig.maxNodes;
                std::vector<std::string> nodeIDsToRemove;

                for (const auto& [nodeIDStr, node] : m_discoveredNodes) {
                    nodeIDsToRemove.push_back(nodeIDStr);
                    if (nodeIDsToRemove.size() >= nodesToRemove) {
                        break;
                    }
                }

                for (const auto& nodeIDStr : nodeIDsToRemove) {
                    m_discoveredNodes.erase(nodeIDStr);
                }

                std::string prunedNodesMessage = "Pruned " + std::to_string(nodeIDsToRemove.size()) + " nodes";
                prunedNodesMessage += ", remaining: " + std::to_string(m_discoveredNodes.size());
                unified_event::logDebug("DHT.Crawler", prunedNodesMessage);
            }

            if (m_discoveredInfoHashes.size() > m_crawlerConfig.maxInfoHashes) {
                // Remove the oldest info hashes
                size_t infoHashesToRemove = m_discoveredInfoHashes.size() - m_crawlerConfig.maxInfoHashes;
                std::vector<std::string> infoHashesToRemoveVec;

                for (const auto& [infoHashStr, _] : m_discoveredInfoHashes) {
                    // Don't remove monitored info hashes
                    if (m_monitoredInfoHashes.find(infoHashStr) == m_monitoredInfoHashes.end()) {
                        infoHashesToRemoveVec.push_back(infoHashStr);
                        if (infoHashesToRemoveVec.size() >= infoHashesToRemove) {
                            break;
                        }
                    }
                }

                for (const auto& infoHashStr : infoHashesToRemoveVec) {
                    m_discoveredInfoHashes.erase(infoHashStr);
                    m_infoHashPeers.erase(infoHashStr);
                }

                std::string prunedHashesMessage = "Pruned " + std::to_string(infoHashesToRemoveVec.size()) + " info hashes";
                prunedHashesMessage += ", remaining: " + std::to_string(m_discoveredInfoHashes.size());
                unified_event::logDebug("DHT.Crawler", prunedHashesMessage);
            }
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }

    // Update statistics
    m_statistics.nodesDiscovered = m_discoveredNodes.size();
    m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
    m_statistics.peersDiscovered = 0;
    for (const auto& [_, peerSet] : m_infoHashPeers) {
        m_statistics.peersDiscovered += peerSet.size();
    }
}

void Crawler::handleNodeDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto nodeDiscoveredEvent = std::dynamic_pointer_cast<unified_event::NodeDiscoveredEvent>(event);
    if (!nodeDiscoveredEvent) {
        return;
    }

    auto node = nodeDiscoveredEvent->getNode();
    if (!node) {
        return;
    }

    try {
        utility::thread::withLock(m_mutex, [this, node, nodeDiscoveredEvent]() {
            // Add the node to our discovered nodes
            std::string nodeIDStr = nodeIDToString(node->getID());
            std::string endpoint = node->getEndpoint().toString();

            // Check if this is a new node
            bool isNewNode = m_discoveredNodes.find(nodeIDStr) == m_discoveredNodes.end();
            m_discoveredNodes[nodeIDStr] = node;

            // Update statistics
            m_statistics.nodesDiscovered = m_discoveredNodes.size();
            m_statistics.nodesResponded++;

            // Log the event with detailed information
            if (isNewNode) {
                // Get bucket information if available
                auto bucketOpt = nodeDiscoveredEvent->getProperty<size_t>("bucket");
                std::string bucketInfo = bucketOpt ? ", bucket: " + std::to_string(*bucketOpt) : "";

                // Get discovery method if available
                auto methodOpt = nodeDiscoveredEvent->getProperty<std::string>("discoveryMethod");
                std::string methodInfo = methodOpt ? ", " + *methodOpt : "";

                std::string logMessage = "Node discovered - ID: " + nodeIDStr;
                logMessage += ", endpoint: " + endpoint + bucketInfo + methodInfo;
                unified_event::logDebug("DHT.Crawler", logMessage);
            }
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

void Crawler::handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto peerDiscoveredEvent = std::dynamic_pointer_cast<unified_event::PeerDiscoveredEvent>(event);
    if (!peerDiscoveredEvent) {
        return;
    }

    auto infoHash = peerDiscoveredEvent->getInfoHash();
    auto peer = peerDiscoveredEvent->getPeer();

    try {
        utility::thread::withLock(m_mutex, [this, infoHash, peer]() {
            // Add the info hash to our discovered info hashes
            std::string infoHashStr = infoHashToString(infoHash);
            std::string peerStr = peer.toString();

            // Check if this is a new info hash or new peer
            bool isNewInfoHash = m_discoveredInfoHashes.find(infoHashStr) == m_discoveredInfoHashes.end();
            bool isNewPeer = m_infoHashPeers[infoHashStr].find(peerStr) == m_infoHashPeers[infoHashStr].end();

            m_discoveredInfoHashes[infoHashStr] = infoHash;
            m_infoHashPeers[infoHashStr].insert(peerStr);

            // Update statistics
            m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
            m_statistics.peersDiscovered = 0;
            for (const auto& infoHashPeersPair : m_infoHashPeers) {
                m_statistics.peersDiscovered += infoHashPeersPair.second.size();
            }

            // Log the event with detailed information
            if (isNewInfoHash) {
                std::string logMessage = "New info hash discovered - Hash: " + infoHashStr;
                unified_event::logDebug("DHT.Crawler", logMessage);

                // We still want to publish the event but without logging
                if (m_eventBus) {
                    auto event = std::make_shared<unified_event::InfoHashDiscoveredEvent>("DHT.Crawler", infoHash);
                    m_eventBus->publish(event);
                }
            }

            if (isNewPeer) {
                // Check if this is a monitored info hash
                bool isMonitored = m_monitoredInfoHashes.find(infoHashStr) != m_monitoredInfoHashes.end();
                std::string monitoredStr = isMonitored ? " (monitored)" : "";

                std::string logMessage = "New peer discovered - Hash: " + infoHashStr;
                logMessage += monitoredStr + ", peer: " + peerStr;
                unified_event::logDebug("DHT.Crawler", logMessage);
            }
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

void Crawler::handleMessageReceivedEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageReceivedEvent = std::dynamic_pointer_cast<unified_event::MessageReceivedEvent>(event);
    if (!messageReceivedEvent) {
        return;
    }

    try {
        utility::thread::withLock(m_mutex, [this, messageReceivedEvent]() {
            // Update statistics
            m_statistics.responsesReceived++;

            // Log detailed information about the message
            auto messageOpt = messageReceivedEvent->getProperty<std::shared_ptr<dht::Message>>("message");
            auto senderOpt = messageReceivedEvent->getProperty<network::EndPoint>("sender");

            if (messageOpt && senderOpt) {
                auto message = *messageOpt;
                auto sender = *senderOpt;

                // Get message type
                std::string messageType;
                if (message->getType() == dht::MessageType::Response) {
                    messageType = "Response";

                    // Check if it's an error response
                    auto errorResponse = std::dynamic_pointer_cast<dht::ErrorMessage>(message);
                    if (errorResponse) {
                        m_statistics.errorsReceived++;
                        messageType = "Error (" + std::to_string(static_cast<int>(errorResponse->getCode())) + ")";
                    }
                } else if (message->getType() == dht::MessageType::Query) {
                    messageType = "Query";
                }

                // Get transaction ID
                std::string transactionID = message->getTransactionID();

                // Log at debug level since these can be very frequent
                std::string logMessage = "Message received - Type: " + messageType;
                logMessage += ", from: " + sender.toString();
                logMessage += ", txID: " + transactionID;
                unified_event::logDebug("DHT.Crawler", logMessage);
            }
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

void Crawler::handleMessageSentEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageSentEvent = std::dynamic_pointer_cast<unified_event::MessageSentEvent>(event);
    if (!messageSentEvent) {
        return;
    }

    try {
        utility::thread::withLock(m_mutex, [this, messageSentEvent]() {
            // Update statistics
            m_statistics.queriesSent++;

            // Log detailed information about the message
            auto messageOpt = messageSentEvent->getProperty<std::shared_ptr<dht::Message>>("message");
            auto recipientOpt = messageSentEvent->getProperty<network::EndPoint>("recipient");

            if (messageOpt && recipientOpt) {
                auto message = *messageOpt;
                auto recipient = *recipientOpt;

                // Get message type and method
                std::string messageInfo;
                if (message->getType() == dht::MessageType::Query) {
                    auto query = std::dynamic_pointer_cast<dht::QueryMessage>(message);
                    if (query) {
                        switch (query->getMethod()) {
                            case dht::QueryMethod::Ping:
                                messageInfo = "Ping";
                                break;
                            case dht::QueryMethod::FindNode:
                                messageInfo = "FindNode";
                                break;
                            case dht::QueryMethod::GetPeers:
                                messageInfo = "GetPeers";
                                break;
                            case dht::QueryMethod::AnnouncePeer:
                                messageInfo = "AnnouncePeer";
                                break;
                            default:
                                messageInfo = "Unknown";
                        }
                    }
                } else if (message->getType() == dht::MessageType::Response) {
                    messageInfo = "Response";
                }

                // Get transaction ID
                std::string transactionID = message->getTransactionID();

                // Log at debug level since these can be very frequent
                std::string logMessage = "Message sent - Type: " + messageInfo;
                logMessage += ", to: " + recipient.toString();
                logMessage += ", txID: " + transactionID;
                unified_event::logDebug("DHT.Crawler", logMessage);
            }
        }, "Crawler::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler", e.what());
    }
}

} // namespace dht_hunter::dht
