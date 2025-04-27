#include "dht_hunter/dht/routing/node_verifier.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include <algorithm>

namespace dht_hunter::dht {

NodeVerifier::NodeVerifier(const DHTConfig& config,
                         const NodeID& nodeID,
                         std::shared_ptr<RoutingTable> routingTable,
                         std::shared_ptr<TransactionManager> transactionManager,
                         std::shared_ptr<MessageSender> messageSender,
                         std::shared_ptr<unified_event::EventBus> eventBus)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_eventBus(eventBus),
      m_running(false) {
}

NodeVerifier::~NodeVerifier() {
    stop();
}

bool NodeVerifier::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    m_running = true;

    // Start the processing thread
    m_processingThread = std::thread(&NodeVerifier::processQueue, this);

    return true;
}

void NodeVerifier::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running) {
            return;
        }

        m_running = false;
    } // Release the lock before joining the thread

    // Wait for the processing thread to finish
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
}

bool NodeVerifier::isRunning() const {
    return m_running;
}

bool NodeVerifier::verifyNode(std::shared_ptr<Node> node, std::function<void(bool)> callback, size_t bucketIndex) {
    if (!node) {
        if (callback) {
            callback(false);
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the node is already in the routing table
    if (m_routingTable && m_routingTable->getNode(node->getID())) {
        if (callback) {
            callback(true);
        }
        return true;
    }

    // Check if the node was recently verified
    std::string nodeIDStr = nodeIDToString(node->getID());
    auto now = std::chrono::steady_clock::now();
    auto it = m_recentlyVerified.find(nodeIDStr);
    if (it != m_recentlyVerified.end() && (now - it->second) < RECENTLY_VERIFIED_EXPIRY) {
        bool result = addVerifiedNode(node);
        if (callback) {
            callback(result);
        }
        return result;
    }

    // Add the node to the verification queue
    VerificationEntry entry;
    entry.node = node;
    entry.queuedTime = now;
    entry.callback = callback;
    entry.bucketIndex = bucketIndex;
    m_verificationQueue.push(entry);
    return true;
}

size_t NodeVerifier::getQueueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_verificationQueue.size();
}

void NodeVerifier::processQueue() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::shared_ptr<Node> nodeToVerify;
        std::function<void(bool)> callback;
        size_t bucketIndex;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Clean up expired entries in the recently verified map
            auto now = std::chrono::steady_clock::now();
            for (auto it = m_recentlyVerified.begin(); it != m_recentlyVerified.end();) {
                if (now - it->second > RECENTLY_VERIFIED_EXPIRY) {
                    it = m_recentlyVerified.erase(it);
                } else {
                    ++it;
                }
            }

            // Check if there are any nodes to verify
            if (m_verificationQueue.empty()) {
                continue;
            }

            // Get the next node to verify
            VerificationEntry entry = m_verificationQueue.front();

            // Check if it's time to verify the node
            if (now - entry.queuedTime < VERIFICATION_WAIT_TIME) {
                continue;
            }

            // Remove the node from the queue
            m_verificationQueue.pop();

            // Store the node, callback, and bucket index for use outside the lock
            nodeToVerify = entry.node;
            callback = entry.callback;
            bucketIndex = entry.bucketIndex;
        }

        // Ping the node to verify it
        if (nodeToVerify) {
            pingNode(nodeToVerify, callback, bucketIndex);
        }
    }
}

void NodeVerifier::pingNode(std::shared_ptr<Node> node, std::function<void(bool)> callback, size_t bucketIndex) {
    if (!m_transactionManager || !m_messageSender) {
        if (callback) {
            callback(false);
        }
        return;
    }

    // Create a ping query
    auto query = std::make_shared<PingQuery>("", m_nodeID);

    // Send the query
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        node->getEndpoint(),
        [this, node, callback, bucketIndex](std::shared_ptr<ResponseMessage> /*response*/, const network::EndPoint& /*sender*/) {
            // Node responded, now we can consider it discovered

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                // Add the node to the recently verified map
                m_recentlyVerified[nodeIDToString(node->getID())] = std::chrono::steady_clock::now();
            }

            // Publish a node discovered event with detailed information
            auto discoveredEvent = std::make_shared<unified_event::NodeDiscoveredEvent>("DHT.RoutingManager", node);

            // Add bucket information
            discoveredEvent->setProperty("bucket", bucketIndex);

            // Add discovery method information
            discoveredEvent->setProperty("discoveryMethod", "via ping response");

            m_eventBus->publish(discoveredEvent);

            // Now add the node to the routing table
            bool result = addVerifiedNode(node);
            if (callback) {
                callback(result);
            }
        },
        [node, callback](std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& /*sender*/) {
            // Node responded with an error, don't add it to the routing table
            if (callback) {
                callback(false);
            }
        },
        [node, callback]() {
            // Node didn't respond, don't add it to the routing table
            if (callback) {
                callback(false);
            }
        });

    if (!transactionID.empty()) {
        m_messageSender->sendQuery(query, node->getEndpoint());
    } else {
        if (callback) {
            callback(false);
        }
    }
}

bool NodeVerifier::addVerifiedNode(std::shared_ptr<Node> node) {
    if (!m_routingTable) {
        return false;
    }

    bool result = m_routingTable->addNode(node);
    if (result) {
    } else {
    }

    return result;
}

} // namespace dht_hunter::dht
