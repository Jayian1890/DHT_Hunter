#include "dht_hunter/dht/crawler/crawler.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/routing/node_lookup.hpp"
#include "dht_hunter/dht/routing/peer_lookup.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include "dht_hunter/unified_event/events/peer_events.hpp"
#include "dht_hunter/unified_event/events/message_events.hpp"
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

    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<Crawler>(new Crawler(
            config, nodeID, routingManager, nodeLookup, peerLookup,
            transactionManager, messageSender, peerStorage, crawlerConfig));
    }

    return s_instance;
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
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool Crawler::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    unified_event::logInfo("DHT.Crawler", "Starting crawler");

    m_running = true;
    m_crawlThread = std::thread(&Crawler::crawlThread, this);

    return true;
}

void Crawler::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running) {
            return;
        }

        unified_event::logInfo("DHT.Crawler", "Stopping crawler");

        m_running = false;
    }

    if (m_crawlThread.joinable()) {
        m_crawlThread.join();
    }
}

bool Crawler::isRunning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_running;
}

CrawlerStatistics Crawler::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

std::vector<std::shared_ptr<Node>> Crawler::getDiscoveredNodes(size_t maxNodes) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<Node>> nodes;
    nodes.reserve(m_discoveredNodes.size());

    for (const auto& [_, node] : m_discoveredNodes) {
        nodes.push_back(node);
        if (maxNodes > 0 && nodes.size() >= maxNodes) {
            break;
        }
    }

    return nodes;
}

std::vector<InfoHash> Crawler::getDiscoveredInfoHashes(size_t maxInfoHashes) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<InfoHash> infoHashes;
    infoHashes.reserve(m_discoveredInfoHashes.size());

    for (const auto& [_, infoHash] : m_discoveredInfoHashes) {
        infoHashes.push_back(infoHash);
        if (maxInfoHashes > 0 && infoHashes.size() >= maxInfoHashes) {
            break;
        }
    }

    return infoHashes;
}

std::vector<network::EndPoint> Crawler::getPeersForInfoHash(const InfoHash& infoHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<network::EndPoint> peers;
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
                network::EndPoint endpoint(address, port);
                peers.push_back(endpoint);
            }
        }
    }

    return peers;
}

void Crawler::monitorInfoHash(const InfoHash& infoHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_monitoredInfoHashes.insert(infoHashToString(infoHash));
}

void Crawler::stopMonitoringInfoHash(const InfoHash& infoHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_monitoredInfoHashes.erase(infoHashToString(infoHash));
}

bool Crawler::isMonitoringInfoHash(const InfoHash& infoHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredInfoHashes.find(infoHashToString(infoHash)) != m_monitoredInfoHashes.end();
}

std::vector<InfoHash> Crawler::getMonitoredInfoHashes() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<InfoHash> infoHashes;
    infoHashes.reserve(m_monitoredInfoHashes.size());

    for (const auto& infoHashStr : m_monitoredInfoHashes) {
        InfoHash infoHash;
        if (infoHashFromString(infoHashStr, infoHash)) {
            infoHashes.push_back(infoHash);
        }
    }

    return infoHashes;
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
    // Discover new nodes
    discoverNodes();

    // Monitor info hashes
    monitorInfoHashes();

    // Update statistics
    updateStatistics();
}

void Crawler::discoverNodes() {
    // Get a random subset of nodes to crawl
    std::vector<std::shared_ptr<Node>> nodesToCrawl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

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
        std::shuffle(allNodes.begin(), allNodes.end(), g);

        // Take the first N nodes
        size_t nodesToTake = std::min(m_crawlerConfig.parallelCrawls, allNodes.size());
        nodesToCrawl.assign(allNodes.begin(), allNodes.begin() + static_cast<long>(nodesToTake));
    }

    // Perform node lookups
    for (const auto& node : nodesToCrawl) {
        if (m_nodeLookup) {
            m_nodeLookup->lookup(node->getID(), [this](const std::vector<std::shared_ptr<Node>>& nodes) {
                std::lock_guard<std::mutex> lock(m_mutex);

                // Add the nodes to our discovered nodes
                for (const auto& node : nodes) {
                    std::string nodeIDStr = nodeIDToString(node->getID());
                    m_discoveredNodes[nodeIDStr] = node;
                }

                // Update statistics
                m_statistics.nodesDiscovered = m_discoveredNodes.size();
            });
        }
    }

    // Generate some random node IDs to look up
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dis(0, 255);

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

        // Perform a node lookup
        if (m_nodeLookup) {
            m_nodeLookup->lookup(randomNodeID, [this](const std::vector<std::shared_ptr<Node>>& nodes) {
                std::lock_guard<std::mutex> lock(m_mutex);

                // Add the nodes to our discovered nodes
                for (const auto& node : nodes) {
                    std::string nodeIDStr = nodeIDToString(node->getID());
                    m_discoveredNodes[nodeIDStr] = node;
                }

                // Update statistics
                m_statistics.nodesDiscovered = m_discoveredNodes.size();
            });
        }
    }
}

void Crawler::monitorInfoHashes() {
    std::vector<InfoHash> infoHashesToMonitor;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Get all monitored info hashes
        for (const auto& infoHashStr : m_monitoredInfoHashes) {
            InfoHash infoHash;
            if (infoHashFromString(infoHashStr, infoHash)) {
                infoHashesToMonitor.push_back(infoHash);
            }
        }
    }

    // Perform peer lookups for each info hash
    for (const auto& infoHash : infoHashesToMonitor) {
        if (m_peerLookup) {
            m_peerLookup->lookup(infoHash, [this, infoHash](const std::vector<network::EndPoint>& peers) {
                std::lock_guard<std::mutex> lock(m_mutex);

                // Add the info hash to our discovered info hashes
                std::string infoHashStr = infoHashToString(infoHash);
                m_discoveredInfoHashes[infoHashStr] = infoHash;

                // Add the peers to our info hash peers
                auto& peerSet = m_infoHashPeers[infoHashStr];
                for (const auto& peer : peers) {
                    peerSet.insert(peer.toString());
                }

                // Update statistics
                m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
                m_statistics.peersDiscovered = 0;
                for (const auto& infoHashPeersPair : m_infoHashPeers) {
                    const auto& peerSetCount = infoHashPeersPair.second;
                    m_statistics.peersDiscovered += peerSetCount.size();
                }
            });
        }
    }

    // Generate some random info hashes to look up
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < m_crawlerConfig.parallelCrawls; ++i) {
        // Generate a random info hash
        std::vector<uint8_t> randomInfoHashVec(20);
        for (size_t j = 0; j < 20; ++j) {
            randomInfoHashVec[j] = static_cast<uint8_t>(dis(g));
        }

        // Create an InfoHash from the vector
        std::array<uint8_t, 20> randomInfoHashArray;
        std::copy_n(randomInfoHashVec.begin(), 20, randomInfoHashArray.begin());
        InfoHash randomInfoHash(randomInfoHashArray);

        // Perform a peer lookup
        if (m_peerLookup) {
            m_peerLookup->lookup(randomInfoHash, [this, randomInfoHash](const std::vector<network::EndPoint>& peers) {
                std::lock_guard<std::mutex> lock(m_mutex);

                // Only add the info hash if we found peers
                if (!peers.empty()) {
                    // Add the info hash to our discovered info hashes
                    std::string infoHashStr = infoHashToString(randomInfoHash);
                    m_discoveredInfoHashes[infoHashStr] = randomInfoHash;

                    // Add the peers to our info hash peers
                    auto& peerSet = m_infoHashPeers[infoHashStr];
                    for (const auto& peer : peers) {
                        peerSet.insert(peer.toString());
                    }

                    // Update statistics
                    m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
                    m_statistics.peersDiscovered = 0;
                    for (const auto& infoHashPeersPair : m_infoHashPeers) {
                        const auto& peerSetCount = infoHashPeersPair.second;
                        m_statistics.peersDiscovered += peerSetCount.size();
                    }
                }
            });
        }
    }
}

void Crawler::updateStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);

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

    std::lock_guard<std::mutex> lock(m_mutex);

    // Add the node to our discovered nodes
    std::string nodeIDStr = nodeIDToString(node->getID());
    m_discoveredNodes[nodeIDStr] = node;

    // Update statistics
    m_statistics.nodesDiscovered = m_discoveredNodes.size();
    m_statistics.nodesResponded++;
}

void Crawler::handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto peerDiscoveredEvent = std::dynamic_pointer_cast<unified_event::PeerDiscoveredEvent>(event);
    if (!peerDiscoveredEvent) {
        return;
    }

    auto infoHash = peerDiscoveredEvent->getInfoHash();
    auto peer = peerDiscoveredEvent->getPeer();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Add the info hash to our discovered info hashes
    std::string infoHashStr = infoHashToString(infoHash);
    m_discoveredInfoHashes[infoHashStr] = infoHash;

    // Add the peer to our info hash peers
    m_infoHashPeers[infoHashStr].insert(peer.toString());

    // Update statistics
    m_statistics.infoHashesDiscovered = m_discoveredInfoHashes.size();
    m_statistics.peersDiscovered = 0;
    for (const auto& [_, peerSet] : m_infoHashPeers) {
        m_statistics.peersDiscovered += peerSet.size();
    }
}

void Crawler::handleMessageReceivedEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageReceivedEvent = std::dynamic_pointer_cast<unified_event::MessageReceivedEvent>(event);
    if (!messageReceivedEvent) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Update statistics
    m_statistics.responsesReceived++;
}

void Crawler::handleMessageSentEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageSentEvent = std::dynamic_pointer_cast<unified_event::MessageSentEvent>(event);
    if (!messageSentEvent) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Update statistics
    m_statistics.queriesSent++;
}

} // namespace dht_hunter::dht
