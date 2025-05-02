#include "dht_hunter/dht/crawler/components/crawler_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include "dht_hunter/unified_event/events/peer_events.hpp"
#include "dht_hunter/unified_event/events/message_events.hpp"
#include <algorithm>
#include <random>
#include <chrono>

namespace dht_hunter::dht::crawler {

// Initialize static members
std::shared_ptr<CrawlerManager> CrawlerManager::s_instance = nullptr;
std::mutex CrawlerManager::s_instanceMutex;

std::shared_ptr<CrawlerManager> CrawlerManager::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingManager> routingManager,
    std::shared_ptr<NodeLookup> nodeLookup,
    std::shared_ptr<PeerLookup> peerLookup,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<PeerStorage> peerStorage,
    const CrawlerConfig& crawlerConfig) {

    // If no crawler config was provided, create one from the configuration manager
    CrawlerConfig effectiveCrawlerConfig = crawlerConfig;
    if (crawlerConfig.getParallelCrawls() == 10 && // Check if it's the default config
        crawlerConfig.getRefreshInterval() == 15 &&
        crawlerConfig.getMaxNodes() == 1000000 &&
        crawlerConfig.getMaxInfoHashes() == 1000000 &&
        crawlerConfig.getAutoStart() == true) {

        auto configManager = utility::config::ConfigurationManager::getInstance();
        effectiveCrawlerConfig = CrawlerConfig(configManager);
    }

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<CrawlerManager>(new CrawlerManager(
                    config, nodeID, routingManager, nodeLookup, peerLookup,
                    transactionManager, messageSender, peerStorage, effectiveCrawlerConfig));
            }
            return s_instance;
        }, "CrawlerManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler.CrawlerManager", e.what());
        return nullptr;
    }
}

CrawlerManager::CrawlerManager(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<RoutingManager> routingManager,
                             std::shared_ptr<NodeLookup> nodeLookup,
                             std::shared_ptr<PeerLookup> peerLookup,
                             std::shared_ptr<TransactionManager> transactionManager,
                             std::shared_ptr<MessageSender> messageSender,
                             std::shared_ptr<PeerStorage> peerStorage,
                             const CrawlerConfig& crawlerConfig)
    : BaseCrawlerComponent("CrawlerManager", config, nodeID),
      m_crawlerConfig(crawlerConfig),
      m_routingManager(routingManager),
      m_nodeLookup(nodeLookup),
      m_peerLookup(peerLookup),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_peerStorage(peerStorage),
      m_rng(std::random_device{}()) {
}

CrawlerManager::~CrawlerManager() {
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
        }, "CrawlerManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

bool CrawlerManager::onInitialize() {
    if (!m_routingManager) {
        unified_event::logError("DHT.Crawler." + m_name, "Routing manager is null");
        return false;
    }

    if (!m_nodeLookup) {
        unified_event::logError("DHT.Crawler." + m_name, "Node lookup is null");
        return false;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Crawler." + m_name, "Peer lookup is null");
        return false;
    }

    if (!m_transactionManager) {
        unified_event::logError("DHT.Crawler." + m_name, "Transaction manager is null");
        return false;
    }

    if (!m_messageSender) {
        unified_event::logError("DHT.Crawler." + m_name, "Message sender is null");
        return false;
    }

    if (!m_peerStorage) {
        unified_event::logError("DHT.Crawler." + m_name, "Peer storage is null");
        return false;
    }

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

    return true;
}

bool CrawlerManager::onStart() {
    // Start the crawl thread
    m_crawlThread = std::thread(&CrawlerManager::crawlThread, this);

    return true;
}

void CrawlerManager::onStop() {
    // Stop the crawl thread
    if (m_crawlThread.joinable()) {
        m_crawlThread.join();
    }
}

void CrawlerManager::onCrawl() {
    // Log the start of a crawl iteration
    unified_event::logDebug("DHT.Crawler." + m_name, "Starting crawl iteration");

    // Discover new nodes
    discoverNodes();

    // Monitor info hashes
    monitorInfoHashes();

    // Update statistics
    updateStatistics();

    // Log the completion of a crawl iteration with statistics
    std::string statsMessage = "Completed crawl iteration - ";
    statsMessage += "Nodes: " + std::to_string(m_statistics.getNodesDiscovered()) + ", ";
    statsMessage += "Info hashes: " + std::to_string(m_statistics.getInfoHashesDiscovered()) + ", ";
    statsMessage += "Peers: " + std::to_string(m_statistics.getPeersDiscovered());
    unified_event::logDebug("DHT.Crawler." + m_name, statsMessage);
}

void CrawlerManager::crawlThread() {
    try {
        while (isRunning()) {
            // Perform a crawl iteration
            crawl();

            // Sleep for the refresh interval
            std::this_thread::sleep_for(std::chrono::seconds(m_crawlerConfig.getRefreshInterval()));
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.Crawler." + m_name, "Exception in crawl thread: " + std::string(e.what()));
    } catch (...) {
        unified_event::logError("DHT.Crawler." + m_name, "Unknown exception in crawl thread");
    }
}

void CrawlerManager::discoverNodes() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Get a random subset of nodes to crawl
            std::vector<std::shared_ptr<Node>> nodesToCrawl;

            // Get all nodes
            std::vector<std::shared_ptr<Node>> allNodes;
            allNodes.reserve(m_discoveredNodes.size());
            for (const auto& [_, node] : m_discoveredNodes) {
                allNodes.push_back(node);
            }

            // If we don't have enough nodes, use the routing table
            if (allNodes.size() < m_crawlerConfig.getParallelCrawls() && m_routingManager) {
                auto routingTableNodes = m_routingManager->getRoutingTable()->getAllNodes();
                allNodes.insert(allNodes.end(), routingTableNodes.begin(), routingTableNodes.end());
            }

            // Shuffle the nodes
            std::shuffle(allNodes.begin(), allNodes.end(), m_rng);

            // Take a subset of nodes
            size_t numToCrawl = std::min(allNodes.size(), m_crawlerConfig.getParallelCrawls());
            for (size_t i = 0; i < numToCrawl; ++i) {
                nodesToCrawl.push_back(allNodes[i]);
            }

            // Crawl the nodes
            for (const auto& node : nodesToCrawl) {
                std::string nodeIDStr = nodeIDToString(node->getID());
                unified_event::logDebug("DHT.Crawler." + m_name, "Crawling node: " + nodeIDStr);

                if (m_nodeLookup) {
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
                                m_statistics.setNodesDiscovered(m_discoveredNodes.size());

                                return newNodesCount;
                            }, "CrawlerManager::m_mutex");

                            unified_event::logDebug("DHT.Crawler." + m_name, "Node lookup for " + nodeIDStr + " completed with " + std::to_string(nodes.size()) + " nodes (" + std::to_string(newNodes) + " new)");
                        } catch (const utility::thread::LockTimeoutException& e) {
                            unified_event::logError("DHT.Crawler." + m_name, e.what());
                        }
                    });
                }
            }

            // Perform random lookups
            std::uniform_int_distribution<uint8_t> dis(0, 255);
            for (size_t i = 0; i < m_crawlerConfig.getParallelCrawls(); ++i) {
                // Generate a random node ID
                std::vector<uint8_t> randomNodeIDVec(20);
                for (size_t j = 0; j < 20; ++j) {
                    randomNodeIDVec[j] = dis(m_rng);
                }

                // Create a NodeID from the vector
                std::array<uint8_t, 20> randomNodeIDArray;
                std::copy_n(randomNodeIDVec.begin(), 20, randomNodeIDArray.begin());
                NodeID randomNodeID(randomNodeIDArray);

                std::string randomNodeIDStr = nodeIDToString(randomNodeID);
                std::string randomLookupMessage = "Looking up random node ID: " + randomNodeIDStr;
                unified_event::logDebug("DHT.Crawler." + m_name, randomLookupMessage);

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
                                m_statistics.setNodesDiscovered(m_discoveredNodes.size());

                                return newNodesCount;
                            }, "CrawlerManager::m_mutex");

                            unified_event::logDebug("DHT.Crawler." + m_name, "Random lookup for " + randomNodeIDStr + " completed with " + std::to_string(nodes.size()) + " nodes (" + std::to_string(newNodes) + " new)");
                        } catch (const utility::thread::LockTimeoutException& e) {
                            unified_event::logError("DHT.Crawler." + m_name, e.what());
                        }
                    });
                }
            }
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::monitorInfoHashes() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Get the info hashes to monitor
            std::vector<InfoHash> infoHashesToMonitor;
            infoHashesToMonitor.reserve(m_monitoredInfoHashes.size());
            for (const auto& infoHashStr : m_monitoredInfoHashes) {
                InfoHash infoHash;
                if (infoHashFromString(infoHashStr, infoHash)) {
                    infoHashesToMonitor.push_back(infoHash);
                }
            }

            // Shuffle the info hashes
            std::shuffle(infoHashesToMonitor.begin(), infoHashesToMonitor.end(), m_rng);

            unified_event::logDebug("DHT.Crawler." + m_name, "Monitoring " + std::to_string(infoHashesToMonitor.size()) + " info hashes");

            // Limit the number of info hashes to monitor in each iteration
            size_t numToMonitor = std::min(infoHashesToMonitor.size(), static_cast<size_t>(10)); // Use a reasonable default

            // Perform peer lookups for a limited number of info hashes
            for (size_t i = 0; i < numToMonitor; ++i) {
                const auto& infoHash = infoHashesToMonitor[i];
                if (m_peerLookup) {
                    std::string infoHashStr = infoHashToString(infoHash);
                    unified_event::logDebug("DHT.Crawler." + m_name, "Looking up peers for info hash: " + infoHashStr);

                    m_peerLookup->lookup(infoHash, [this, infoHashStr, infoHash](const std::vector<EndPoint>& peers) {
                        try {
                            size_t newPeers = utility::thread::withLock(m_mutex, [this, &peers, &infoHashStr, &infoHash]() {
                                size_t newPeersCount = 0;

                                // Add the info hash to our discovered info hashes
                                m_discoveredInfoHashes[infoHashStr] = infoHash;

                                // Add the peers to our info hash peers
                                auto& infoHashPeers = m_infoHashPeers[infoHashStr];
                                for (const auto& peer : peers) {
                                    std::string peerStr = peer.toString();
                                    bool isNew = infoHashPeers.find(peerStr) == infoHashPeers.end();

                                    infoHashPeers.insert(peerStr);

                                    if (isNew) {
                                        newPeersCount++;
                                    }
                                }

                                // Update statistics
                                m_statistics.setInfoHashesDiscovered(m_discoveredInfoHashes.size());
                                m_statistics.setPeersDiscovered(m_statistics.getPeersDiscovered() + newPeersCount);

                                return newPeersCount;
                            }, "CrawlerManager::m_mutex");

                            unified_event::logDebug("DHT.Crawler." + m_name, "Peer lookup for " + infoHashStr + " completed with " + std::to_string(peers.size()) + " peers (" + std::to_string(newPeers) + " new)");
                        } catch (const utility::thread::LockTimeoutException& e) {
                            unified_event::logError("DHT.Crawler." + m_name, e.what());
                        }
                    });
                }
            }
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::updateStatistics() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Update the statistics
            m_statistics.setNodesDiscovered(m_discoveredNodes.size());
            m_statistics.setInfoHashesDiscovered(m_discoveredInfoHashes.size());

            // Count the total number of peers
            size_t totalPeers = 0;
            for (const auto& [_, peers] : m_infoHashPeers) {
                totalPeers += peers.size();
            }
            m_statistics.setPeersDiscovered(totalPeers);
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::handleNodeDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    try {
        utility::thread::withLock(m_mutex, [this, &event]() {
            auto nodeDiscoveredEvent = std::dynamic_pointer_cast<unified_event::NodeDiscoveredEvent>(event);
            if (!nodeDiscoveredEvent) {
                return;
            }

            // Get the node ID and endpoint
            auto nodeIDOpt = nodeDiscoveredEvent->getProperty<NodeID>("nodeID");
            auto endpointOpt = nodeDiscoveredEvent->getProperty<EndPoint>("endpoint");
            if (!nodeIDOpt || !endpointOpt) {
                return;
            }

            const auto& nodeID = *nodeIDOpt;
            const auto& endpoint = *endpointOpt;

            // Create a node
            auto node = std::make_shared<Node>(nodeID, endpoint);

            // Add the node to our discovered nodes
            std::string nodeIDStr = nodeIDToString(nodeID);
            bool isNewNode = m_discoveredNodes.find(nodeIDStr) == m_discoveredNodes.end();
            m_discoveredNodes[nodeIDStr] = node;

            // Update statistics
            if (isNewNode) {
                m_statistics.incrementNodesDiscovered();
            }

            // Log the event with detailed information
            if (isNewNode) {
                // Get bucket information if available
                auto bucketOpt = nodeDiscoveredEvent->getProperty<size_t>("bucket");
                std::string bucketInfo = bucketOpt ? ", bucket: " + std::to_string(*bucketOpt) : "";

                // Get discovery method if available
                auto methodOpt = nodeDiscoveredEvent->getProperty<std::string>("discoveryMethod");
                std::string methodInfo = methodOpt ? ", " + *methodOpt : "";

                std::string logMessage = "Node discovered - ID: " + nodeIDStr;
                logMessage += ", endpoint: " + endpoint.toString() + bucketInfo + methodInfo;
                unified_event::logInfo("DHT.Crawler." + m_name, logMessage);
            }
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    try {
        utility::thread::withLock(m_mutex, [this, &event]() {
            auto peerDiscoveredEvent = std::dynamic_pointer_cast<unified_event::PeerDiscoveredEvent>(event);
            if (!peerDiscoveredEvent) {
                return;
            }

            // Get the info hash and endpoint
            auto infoHashOpt = peerDiscoveredEvent->getProperty<InfoHash>("infoHash");
            auto endpointOpt = peerDiscoveredEvent->getProperty<EndPoint>("endpoint");
            if (!infoHashOpt || !endpointOpt) {
                return;
            }

            const auto& infoHash = *infoHashOpt;
            const auto& endpoint = *endpointOpt;

            // Add the info hash to our discovered info hashes
            std::string infoHashStr = infoHashToString(infoHash);
            m_discoveredInfoHashes[infoHashStr] = infoHash;

            // Add the peer to our info hash peers
            std::string peerStr = endpoint.toString();
            auto& infoHashPeers = m_infoHashPeers[infoHashStr];
            bool isNewPeer = infoHashPeers.find(peerStr) == infoHashPeers.end();
            infoHashPeers.insert(peerStr);

            // Update statistics
            if (isNewPeer) {
                m_statistics.incrementPeersDiscovered();
            }

            // Log the event with detailed information
            if (isNewPeer) {
                std::string logMessage = "Peer discovered - Info hash: " + infoHashStr;
                logMessage += ", endpoint: " + peerStr;
                unified_event::logInfo("DHT.Crawler." + m_name, logMessage);
            }
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::handleMessageReceivedEvent(const std::shared_ptr<unified_event::Event>& event) {
    try {
        utility::thread::withLock(m_mutex, [this, &event]() {
            auto messageReceivedEvent = std::dynamic_pointer_cast<unified_event::MessageReceivedEvent>(event);
            if (!messageReceivedEvent) {
                return;
            }

            // Update statistics
            m_statistics.incrementResponsesReceived();
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::handleMessageSentEvent(const std::shared_ptr<unified_event::Event>& event) {
    try {
        utility::thread::withLock(m_mutex, [this, &event]() {
            auto messageSentEvent = std::dynamic_pointer_cast<unified_event::MessageSentEvent>(event);
            if (!messageSentEvent) {
                return;
            }

            // Update statistics
            m_statistics.incrementQueriesSent();
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

CrawlerStatistics CrawlerManager::getStatistics() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_statistics;
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
        return CrawlerStatistics();
    }
}

std::vector<std::shared_ptr<Node>> CrawlerManager::getDiscoveredNodes(size_t maxNodes) const {
    try {
        return utility::thread::withLock(m_mutex, [this, maxNodes]() {
            std::vector<std::shared_ptr<Node>> nodes;
            nodes.reserve(m_discoveredNodes.size());
            for (const auto& [_, node] : m_discoveredNodes) {
                nodes.push_back(node);
            }

            // Limit the number of nodes
            if (maxNodes > 0 && nodes.size() > maxNodes) {
                nodes.resize(maxNodes);
            }

            return nodes;
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
        return {};
    }
}

std::vector<InfoHash> CrawlerManager::getDiscoveredInfoHashes(size_t maxInfoHashes) const {
    try {
        return utility::thread::withLock(m_mutex, [this, maxInfoHashes]() {
            std::vector<InfoHash> infoHashes;
            infoHashes.reserve(m_discoveredInfoHashes.size());
            for (const auto& [_, infoHash] : m_discoveredInfoHashes) {
                infoHashes.push_back(infoHash);
            }

            // Limit the number of info hashes
            if (maxInfoHashes > 0 && infoHashes.size() > maxInfoHashes) {
                infoHashes.resize(maxInfoHashes);
            }

            return infoHashes;
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
        return {};
    }
}

std::vector<EndPoint> CrawlerManager::getPeersForInfoHash(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            std::vector<EndPoint> peers;
            std::string infoHashStr = infoHashToString(infoHash);
            auto it = m_infoHashPeers.find(infoHashStr);
            if (it != m_infoHashPeers.end()) {
                peers.reserve(it->second.size());
                for (const auto& peerStr : it->second) {
                    // Parse the endpoint string (format: "ip:port")
                    size_t colonPos = peerStr.find(':');
                    if (colonPos != std::string::npos) {
                        std::string ip = peerStr.substr(0, colonPos);
                        uint16_t port = static_cast<uint16_t>(std::stoi(peerStr.substr(colonPos + 1)));
                        peers.push_back(EndPoint(network::NetworkAddress(ip), port));
                    }
                }
            }
            return peers;
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
        return {};
    }
}

void CrawlerManager::monitorInfoHash(const InfoHash& infoHash) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash]() {
            std::string infoHashStr = infoHashToString(infoHash);
            m_monitoredInfoHashes.insert(infoHashStr);
            unified_event::logInfo("DHT.Crawler." + m_name, "Monitoring info hash: " + infoHashStr);
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

void CrawlerManager::stopMonitoringInfoHash(const InfoHash& infoHash) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash]() {
            std::string infoHashStr = infoHashToString(infoHash);
            m_monitoredInfoHashes.erase(infoHashStr);
            unified_event::logInfo("DHT.Crawler." + m_name, "Stopped monitoring info hash: " + infoHashStr);
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
    }
}

bool CrawlerManager::isMonitoringInfoHash(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            std::string infoHashStr = infoHashToString(infoHash);
            return m_monitoredInfoHashes.find(infoHashStr) != m_monitoredInfoHashes.end();
        }, "CrawlerManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Crawler." + m_name, e.what());
        return false;
    }
}

} // namespace dht_hunter::dht::crawler
