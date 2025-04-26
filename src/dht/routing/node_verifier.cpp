#include "dht_hunter/dht/routing/node_verifier.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include <algorithm>

namespace dht_hunter::dht {

NodeVerifier::NodeVerifier(const DHTConfig& config,
                         const NodeID& nodeID,
                         std::shared_ptr<RoutingTable> routingTable,
                         std::shared_ptr<TransactionManager> transactionManager,
                         std::shared_ptr<MessageSender> messageSender)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_running(false),
      m_logger(event::Logger::forComponent("DHT.NodeVerifier")) {
}

NodeVerifier::~NodeVerifier() {
    stop();
}

bool NodeVerifier::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Node verifier already running");
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

    m_logger.info("Node verifier stopped");
}

bool NodeVerifier::isRunning() const {
    return m_running;
}

bool NodeVerifier::verifyNode(std::shared_ptr<Node> node, std::function<void(bool)> callback) {
    if (!node) {
        m_logger.error("Cannot verify null node");
        if (callback) {
            callback(false);
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the node is already in the routing table
    if (m_routingTable && m_routingTable->getNode(node->getID())) {
        m_logger.debug("Node {} already in routing table", nodeIDToString(node->getID()));
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
        m_logger.debug("Node {} was recently verified", nodeIDStr);
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
    m_verificationQueue.push(entry);

    m_logger.debug("Added node {} to verification queue", nodeIDStr);
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

            // Store the node and callback for use outside the lock
            nodeToVerify = entry.node;
            callback = entry.callback;
        }

        // Ping the node to verify it
        if (nodeToVerify) {
            pingNode(nodeToVerify, callback);
        }
    }
}

void NodeVerifier::pingNode(std::shared_ptr<Node> node, std::function<void(bool)> callback) {
    if (!m_transactionManager || !m_messageSender) {
        m_logger.error("Cannot ping node: transaction manager or message sender not available");
        if (callback) {
            callback(false);
        }
        return;
    }

    m_logger.debug("Pinging node {} at {}", nodeIDToString(node->getID()), node->getEndpoint().toString());

    // Create a ping query
    auto query = std::make_shared<PingQuery>("", m_nodeID);

    // Send the query
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        node->getEndpoint(),
        [this, node, callback](std::shared_ptr<ResponseMessage> /*response*/, const network::EndPoint& /*sender*/) {
            // Node responded, add it to the routing table
            m_logger.debug("Node {} responded to ping", nodeIDToString(node->getID()));
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                // Add the node to the recently verified map
                m_recentlyVerified[nodeIDToString(node->getID())] = std::chrono::steady_clock::now();
            }
            
            bool result = addVerifiedNode(node);
            if (callback) {
                callback(result);
            }
        },
        [this, node, callback](std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& /*sender*/) {
            // Node responded with an error, don't add it to the routing table
            m_logger.debug("Node {} responded with error", nodeIDToString(node->getID()));
            if (callback) {
                callback(false);
            }
        },
        [this, node, callback]() {
            // Node didn't respond, don't add it to the routing table
            m_logger.debug("Node {} didn't respond to ping", nodeIDToString(node->getID()));
            if (callback) {
                callback(false);
            }
        });

    if (!transactionID.empty()) {
        m_messageSender->sendQuery(query, node->getEndpoint());
    } else {
        m_logger.error("Failed to create transaction for ping");
        if (callback) {
            callback(false);
        }
    }
}

bool NodeVerifier::addVerifiedNode(std::shared_ptr<Node> node) {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    bool result = m_routingTable->addNode(node);
    if (result) {
        m_logger.debug("Added verified node {} to routing table", nodeIDToString(node->getID()));
    } else {
        m_logger.debug("Failed to add verified node {} to routing table", nodeIDToString(node->getID()));
    }

    return result;
}

} // namespace dht_hunter::dht
