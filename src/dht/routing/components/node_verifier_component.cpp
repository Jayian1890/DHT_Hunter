#include "dht_hunter/dht/routing/components/node_verifier_component.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include <algorithm>
#include <random>

namespace dht_hunter::dht::routing {

NodeVerifierComponent::NodeVerifierComponent(const DHTConfig& config,
                                           const NodeID& nodeID,
                                           std::shared_ptr<RoutingTable> routingTable,
                                           std::shared_ptr<TransactionManager> transactionManager,
                                           std::shared_ptr<MessageSender> messageSender)
    : BaseRoutingComponent("NodeVerifier", config, nodeID, routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_processingThreadRunning(false) {
}

NodeVerifierComponent::~NodeVerifierComponent() {
    onStop();
}

bool NodeVerifierComponent::onInitialize() {
    if (!m_transactionManager) {
        unified_event::logError("DHT.Routing." + m_name, "Transaction manager is null");
        return false;
    }

    if (!m_messageSender) {
        unified_event::logError("DHT.Routing." + m_name, "Message sender is null");
        return false;
    }

    return true;
}

bool NodeVerifierComponent::onStart() {
    std::lock_guard<std::mutex> lock(m_processingMutex);

    if (m_processingThreadRunning) {
        return true;
    }

    m_processingThreadRunning = true;
    m_processingThread = std::thread(&NodeVerifierComponent::processQueue, this);
    return true;
}

void NodeVerifierComponent::onStop() {
    {
        std::lock_guard<std::mutex> lock(m_processingMutex);

        if (!m_processingThreadRunning) {
            return;
        }

        m_processingThreadRunning = false;
    }

    m_processingCondition.notify_all();

    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
}

bool NodeVerifierComponent::verifyNode(std::shared_ptr<Node> node, std::function<void(bool)> callback, size_t bucketIndex) {
    if (!node) {
        unified_event::logError("DHT.Routing." + m_name, "Node is null");
        return false;
    }

    // Check if the node is already in the routing table
    if (m_routingTable) {
        auto nodes = m_routingTable->getAllNodes();
        auto it = std::find_if(nodes.begin(), nodes.end(), [&node](const auto& n) {
            return n->getID() == node->getID();
        });

        if (it != nodes.end()) {
            unified_event::logDebug("DHT.Routing." + m_name, "Node " + nodeIDToString(node->getID()) + " is already in the routing table");

            // Update the node's last seen time
            (*it)->updateLastSeen();
        }

        if (callback) {
            callback(true);
        }
        return true;
    }

    // Check if the node was recently verified
    std::string nodeKey = nodeIDToString(node->getID());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_recentlyVerified.find(nodeKey);
        if (it != m_recentlyVerified.end()) {
            auto now = std::chrono::steady_clock::now();
            if (now - it->second < RECENTLY_VERIFIED_EXPIRY) {
                unified_event::logDebug("DHT.Routing." + m_name, "Node " + nodeIDToString(node->getID()) + " was recently verified");
                if (callback) {
                    callback(true);
                }
                return true;
            }
        }
    }

    // Add the node to the verification queue
    {
        std::lock_guard<std::mutex> lock(m_processingMutex);
        VerificationEntry entry;
        entry.node = node;
        entry.queuedTime = std::chrono::steady_clock::now();
        entry.callback = callback;
        entry.bucketIndex = bucketIndex;
        m_verificationQueue.push(entry);
    }

    m_processingCondition.notify_one();
    return true;
}

size_t NodeVerifierComponent::getQueueSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_processingMutex));
    return m_verificationQueue.size();
}

void NodeVerifierComponent::processQueue() {
    try {
        while (m_processingThreadRunning) {
            VerificationEntry entry;
            {
                std::unique_lock<std::mutex> lock(m_processingMutex);
                m_processingCondition.wait(lock, [this] { return !m_processingThreadRunning || !m_verificationQueue.empty(); });

                if (!m_processingThreadRunning) {
                    break;
                }

                if (m_verificationQueue.empty()) {
                    continue;
                }

                entry = m_verificationQueue.front();
                m_verificationQueue.pop();
            }

            // Wait a bit before pinging to avoid overwhelming the node
            auto now = std::chrono::steady_clock::now();
            auto waitTime = entry.queuedTime + VERIFICATION_WAIT_TIME;
            if (now < waitTime) {
                std::this_thread::sleep_for(waitTime - now);
            }

            // Ping the node
            pingNode(entry.node, entry.callback, entry.bucketIndex);
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.Routing." + m_name, "Exception in processing thread: " + std::string(e.what()));
    } catch (...) {
        unified_event::logError("DHT.Routing." + m_name, "Unknown exception in processing thread");
    }
}

void NodeVerifierComponent::pingNode(std::shared_ptr<Node> node, std::function<void(bool)> callback, size_t /*bucketIndex*/) {
    if (!node) {
        if (callback) {
            callback(false);
        }
        return;
    }

    // Generate a random transaction ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::string transactionID;
    for (int i = 0; i < 4; ++i) {
        transactionID.push_back(static_cast<char>(dis(gen)));
    }

    // Create a ping query
    auto pingQuery = std::make_shared<PingQuery>(transactionID, m_nodeID);

    // Send the ping query
    if (m_messageSender && m_transactionManager) {
        // Create a transaction
        auto responseCallback = [this, node, callback](std::shared_ptr<ResponseMessage> response, const network::EndPoint&) {
            bool success = false;

            if (response) {
                // Add the node to the routing table
                success = addVerifiedNode(node);

                // Add the node to the recently verified list
                if (success) {
                    std::string nodeKey = nodeIDToString(node->getID());
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_recentlyVerified[nodeKey] = std::chrono::steady_clock::now();
                }
            }

            // Call the callback
            if (callback) {
                callback(success);
            }
        };

        auto errorCallback = [callback](std::shared_ptr<ErrorMessage>, const network::EndPoint&) {
            // Call the callback with failure
            if (callback) {
                callback(false);
            }
        };

        auto timeoutCallback = [callback]() {
            // Call the callback with failure
            if (callback) {
                callback(false);
            }
        };

        // Create the transaction
        m_transactionManager->createTransaction(pingQuery, node->getEndpoint(), responseCallback, errorCallback, timeoutCallback);

        // Send the query
        if (!m_messageSender->sendQuery(pingQuery, node->getEndpoint())) {
            // Call the callback with failure
            if (callback) {
                callback(false);
            }
        }
    } else {
        if (callback) {
            callback(false);
        }
    }
}

bool NodeVerifierComponent::addVerifiedNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    if (!m_routingTable) {
        return false;
    }

    // Check if the node ID is valid
    if (!node->getID().isValid()) {
        unified_event::logWarning("DHT.Routing." + m_name, "Attempted to add node with invalid ID");
        return false;
    }

    // Check if we already have this node in the routing table
    auto nodes = m_routingTable->getAllNodes();
    auto it = std::find_if(nodes.begin(), nodes.end(), [&node](const auto& n) {
        return n->getID() == node->getID();
    });

    if (it != nodes.end()) {
        // Node already exists, just update its last seen time
        (*it)->updateLastSeen();
        unified_event::logDebug("DHT.Routing." + m_name, "Updated existing node " + nodeIDToString(node->getID()) + " in routing table");
        return true;
    }

    // Add the node to the routing table
    bool result = m_routingTable->addNode(node);

    if (result) {
        unified_event::logDebug("DHT.Routing." + m_name, "Added verified node " + nodeIDToString(node->getID()) + " to routing table");
    }

    return result;
}

} // namespace dht_hunter::dht::routing
