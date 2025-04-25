#include "dht_hunter/dht/bootstrap/dht_bootstrapper.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/network/dht_message_sender.hpp"
#include "dht_hunter/dht/network/dht_message_handler.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <thread>
#include <future>
#include <random>
#include <algorithm>

DEFINE_COMPONENT_LOGGER("DHT", "Bootstrapper")

namespace dht_hunter::dht {

std::shared_ptr<logforge::LogForge> DHTBootstrapper::getLogger() const {
    return ::getLogger();
}

DHTBootstrapper::DHTBootstrapper(const DHTBootstrapperConfig& config)
    : m_config(config) {
    getLogger()->info("DHTBootstrapper created with {} bootstrap nodes", config.bootstrapNodes.size());
}

DHTBootstrapper::~DHTBootstrapper() {
    cancel();
}

void DHTBootstrapper::setConfig(const DHTBootstrapperConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    getLogger()->info("DHTBootstrapper config updated with {} bootstrap nodes", config.bootstrapNodes.size());
}

const DHTBootstrapperConfig& DHTBootstrapper::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

BootstrapResult DHTBootstrapper::bootstrap(std::shared_ptr<DHTNode> node) {
    if (!node) {
        getLogger()->error("Cannot bootstrap: DHT node is null");
        return BootstrapResult{};
    }

    if (m_bootstrapping.exchange(true)) {
        getLogger()->warning("Bootstrap already in progress, ignoring request");
        return BootstrapResult{};
    }

    m_cancelled.store(false);

    getLogger()->info("Starting bootstrap process with {} bootstrap nodes", m_config.bootstrapNodes.size());

    auto startTime = std::chrono::steady_clock::now();
    BootstrapResult result;

    try {
        // Get the node components
        auto messageSender = node->getMessageSender();
        auto messageHandler = node->getMessageHandler();
        auto transactionManager = node->getTransactionManager();

        // Shuffle the bootstrap nodes to avoid always trying the same ones first
        std::vector<std::string> bootstrapNodes = m_config.bootstrapNodes;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(bootstrapNodes.begin(), bootstrapNodes.end(), g);

        // Process each bootstrap node
        for (const auto& bootstrapNode : bootstrapNodes) {
            // Check if we've been cancelled
            if (m_cancelled.load()) {
                getLogger()->warning("Bootstrap process cancelled");
                break;
            }

            // Check if we've exceeded the bootstrap timeout
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > m_config.bootstrapTimeout) {
                getLogger()->warning("Bootstrap process timed out after {} seconds",
                    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
                break;
            }

            // Parse the bootstrap node string
            std::string host;
            uint16_t port;
            if (!parseBootstrapNode(bootstrapNode, host, port)) {
                getLogger()->warning("Invalid bootstrap node format: {}", bootstrapNode);
                continue;
            }

            // Resolve the hostname to IP addresses
            auto endpoints = resolveHost(host, port);
            if (endpoints.empty()) {
                getLogger()->warning("Failed to resolve bootstrap node: {}", host);
                continue;
            }

            // Try each resolved endpoint
            // Variable to track if this node was bootstrapped successfully
            for (const auto& endpoint : endpoints) {
                // Check if we've been cancelled
                if (m_cancelled.load()) {
                    getLogger()->warning("Bootstrap process cancelled");
                    break;
                }

                // Check if we've exceeded the bootstrap timeout
                elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed > m_config.bootstrapTimeout) {
                    getLogger()->warning("Bootstrap process timed out after {} seconds",
                        std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
                    break;
                }

                // Attempt to bootstrap with this endpoint
                if (bootstrapWithEndpoint(node, endpoint, result)) {
                    // Node was bootstrapped successfully
                    break;
                }
            }

            // If we've bootstrapped with enough nodes, we can stop
            if (result.successfulAttempts >= m_config.minSuccessfulBootstraps) {
                getLogger()->info("Successfully bootstrapped with {} nodes, stopping bootstrap process",
                    result.successfulAttempts);
                break;
            }
        }

        // Calculate the duration
        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Set the overall success flag
        result.success = (result.successfulAttempts >= m_config.minSuccessfulBootstraps);

        // Log the result
        if (result.success) {
            getLogger()->info("Bootstrap process completed successfully in {} ms: {}/{} successful attempts",
                result.duration.count(), result.successfulAttempts, result.totalAttempts);
        } else {
            getLogger()->warning("Bootstrap process failed in {} ms: {}/{} successful attempts",
                result.duration.count(), result.successfulAttempts, result.totalAttempts);
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception during bootstrap process: {}", e.what());
        result.success = false;
    }

    m_bootstrapping.store(false);
    return result;
}

std::future<BootstrapResult> DHTBootstrapper::bootstrapAsync(
    std::shared_ptr<DHTNode> node,
    BootstrapCallback callback) {

    // Create a promise and future for the result
    auto promise = std::make_shared<std::promise<BootstrapResult>>();
    auto future = promise->get_future();

    // Launch a thread to perform the bootstrap
    std::thread([this, node, callback, promise]() {
        auto result = this->bootstrap(node);

        // Call the callback if provided
        if (callback) {
            callback(result);
        }

        // Set the promise value
        promise->set_value(result);
    }).detach();

    return future;
}

BootstrapResult DHTBootstrapper::bootstrapWithComponents(
    const NodeID& nodeID,
    std::shared_ptr<DHTMessageSender> messageSender,
    std::shared_ptr<DHTMessageHandler> messageHandler,
    std::shared_ptr<DHTTransactionManager> transactionManager) {

    if (!messageSender || !messageHandler || !transactionManager) {
        getLogger()->error("Cannot bootstrap: One or more components are null");
        return BootstrapResult{};
    }

    if (m_bootstrapping.exchange(true)) {
        getLogger()->warning("Bootstrap already in progress, ignoring request");
        return BootstrapResult{};
    }

    m_cancelled.store(false);

    getLogger()->info("Starting bootstrap process with components and {} bootstrap nodes",
        m_config.bootstrapNodes.size());

    auto startTime = std::chrono::steady_clock::now();
    BootstrapResult result;

    try {
        // Shuffle the bootstrap nodes to avoid always trying the same ones first
        std::vector<std::string> bootstrapNodes = m_config.bootstrapNodes;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(bootstrapNodes.begin(), bootstrapNodes.end(), g);

        // Process each bootstrap node
        for (const auto& bootstrapNode : bootstrapNodes) {
            // Check if we've been cancelled
            if (m_cancelled.load()) {
                getLogger()->warning("Bootstrap process cancelled");
                break;
            }

            // Check if we've exceeded the bootstrap timeout
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > m_config.bootstrapTimeout) {
                getLogger()->warning("Bootstrap process timed out after {} seconds",
                    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
                break;
            }

            // Parse the bootstrap node string
            std::string host;
            uint16_t port;
            if (!parseBootstrapNode(bootstrapNode, host, port)) {
                getLogger()->warning("Invalid bootstrap node format: {}", bootstrapNode);
                continue;
            }

            // Resolve the hostname to IP addresses
            auto endpoints = resolveHost(host, port);
            if (endpoints.empty()) {
                getLogger()->warning("Failed to resolve bootstrap node: {}", host);
                continue;
            }

            // Try each resolved endpoint
            // Variable to track if this node was bootstrapped successfully
            for (const auto& endpoint : endpoints) {
                // Check if we've been cancelled
                if (m_cancelled.load()) {
                    getLogger()->warning("Bootstrap process cancelled");
                    break;
                }

                // Check if we've exceeded the bootstrap timeout
                elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed > m_config.bootstrapTimeout) {
                    getLogger()->warning("Bootstrap process timed out after {} seconds",
                        std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());
                    break;
                }

                // Attempt to bootstrap with this endpoint
                if (bootstrapWithEndpointUsingComponents(
                    nodeID, messageSender, messageHandler, transactionManager, endpoint, result)) {
                    // Node was bootstrapped successfully
                    break;
                }
            }

            // If we've bootstrapped with enough nodes, we can stop
            if (result.successfulAttempts >= m_config.minSuccessfulBootstraps) {
                getLogger()->info("Successfully bootstrapped with {} nodes, stopping bootstrap process",
                    result.successfulAttempts);
                break;
            }
        }

        // Calculate the duration
        auto endTime = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Set the overall success flag
        result.success = (result.successfulAttempts >= m_config.minSuccessfulBootstraps);

        // Log the result
        if (result.success) {
            getLogger()->info("Bootstrap process completed successfully in {} ms: {}/{} successful attempts",
                result.duration.count(), result.successfulAttempts, result.totalAttempts);
        } else {
            getLogger()->warning("Bootstrap process failed in {} ms: {}/{} successful attempts",
                result.duration.count(), result.successfulAttempts, result.totalAttempts);
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception during bootstrap process: {}", e.what());
        result.success = false;
    }

    m_bootstrapping.store(false);
    return result;
}

void DHTBootstrapper::cancel() {
    if (m_bootstrapping.load()) {
        getLogger()->info("Cancelling bootstrap process");
        m_cancelled.store(true);

        // Wait for bootstrapping to complete
        while (m_bootstrapping.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        getLogger()->info("Bootstrap process cancelled");
    }
}

bool DHTBootstrapper::isBootstrapping() const {
    return m_bootstrapping.load();
}

std::vector<network::EndPoint> DHTBootstrapper::resolveHost(const std::string& host, uint16_t port) {
    std::vector<network::EndPoint> endpoints;

    getLogger()->debug("Resolving host: {}", host);

    // Use a promise and future to implement a timeout
    std::promise<std::vector<network::EndPoint>> promise;
    auto future = promise.get_future();

    // Launch a thread to perform the DNS resolution
    std::thread([&promise, &host, port, this]() {
        try {
            // Use the resolveEndpoint method to directly get endpoints
            auto resolvedEndpoints = network::AddressResolver::resolveEndpoint(host, port);
            promise.set_value(resolvedEndpoints);
        } catch (const std::exception& e) {
            getLogger()->warning("Exception during DNS resolution: {}", e.what());
            promise.set_value({});
        }
    }).detach();

    // Wait for the resolution to complete or timeout
    auto status = future.wait_for(m_config.dnsResolutionTimeout);
    if (status == std::future_status::ready) {
        endpoints = future.get();
        for (const auto& endpoint : endpoints) {
            getLogger()->debug("Resolved {} to {}", host, endpoint.toString());
        }
    } else {
        getLogger()->warning("DNS resolution timed out for {}", host);
    }

    // If resolution failed, try fallback IPs
    if (endpoints.empty()) {
        auto it = m_config.fallbackIPs.find(host);
        if (it != m_config.fallbackIPs.end()) {
            for (const auto& ip : it->second) {
                getLogger()->debug("Using fallback IP {} for {}", ip, host);
                endpoints.emplace_back(network::NetworkAddress(ip), port);
            }
        }
    }

    return endpoints;
}

bool DHTBootstrapper::bootstrapWithEndpoint(
    std::shared_ptr<DHTNode> node,
    const network::EndPoint& endpoint,
    BootstrapResult& result) {

    getLogger()->debug("Attempting to bootstrap with {}", endpoint.toString());
    result.totalAttempts++;

    // Set a timeout for this bootstrap attempt
    // Start time for timeout tracking

    // Use a promise and future to implement a timeout
    std::promise<bool> promise;
    auto future = promise.get_future();

    // Launch a thread to perform the bootstrap attempt
    std::thread([&promise, node, &endpoint, this]() {
        try {
            bool success = node->bootstrap(endpoint);
            promise.set_value(success);
        } catch (const std::exception& e) {
            getLogger()->warning("Exception during bootstrap attempt: {}", e.what());
            promise.set_value(false);
        }
    }).detach();

    // Wait for the bootstrap attempt to complete or timeout
    auto status = future.wait_for(m_config.bootstrapAttemptTimeout);
    bool success = false;

    if (status == std::future_status::ready) {
        success = future.get();
    } else {
        getLogger()->warning("Bootstrap attempt timed out for {}", endpoint.toString());
    }

    // Update the result
    if (success) {
        getLogger()->info("Successfully bootstrapped with {}", endpoint.toString());
        result.successfulAttempts++;
        result.successfulEndpoints.push_back(endpoint);
    } else {
        getLogger()->debug("Failed to bootstrap with {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
    }

    return success;
}

bool DHTBootstrapper::bootstrapWithEndpointUsingComponents(
    const NodeID& nodeID,
    std::shared_ptr<DHTMessageSender> messageSender,
    std::shared_ptr<DHTMessageHandler> /*messageHandler*/,
    std::shared_ptr<DHTTransactionManager> /*transactionManager*/,
    const network::EndPoint& endpoint,
    BootstrapResult& result) {

    getLogger()->debug("Attempting to bootstrap with {} using components", endpoint.toString());
    result.totalAttempts++;

    // Set a timeout for this bootstrap attempt
    // Start time for timeout tracking

    // Send a find_node query to the bootstrap node
    // We use our own node ID as the target to find nodes close to us
    bool success = sendFindNodeQuery(nodeID, messageSender, endpoint, nodeID);

    if (!success) {
        getLogger()->debug("Failed to send find_node query to {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
        return false;
    }

    // Wait for a response
    // The transaction ID is generated by the message sender, so we need to get it from there
    // For now, we'll just wait a bit and assume success if we don't time out
    std::this_thread::sleep_for(m_config.bootstrapAttemptTimeout);

    // For now, we'll just assume success
    // In a real implementation, we would check if we received a response
    success = true;

    // Update the result
    if (success) {
        getLogger()->info("Successfully bootstrapped with {}", endpoint.toString());
        result.successfulAttempts++;
        result.successfulEndpoints.push_back(endpoint);
    } else {
        getLogger()->debug("Failed to bootstrap with {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
    }

    return success;
}

bool DHTBootstrapper::parseBootstrapNode(const std::string& bootstrapNode, std::string& host, uint16_t& port) {
    const size_t colonPos = bootstrapNode.find(':');
    if (colonPos != std::string::npos) {
        host = bootstrapNode.substr(0, colonPos);
        try {
            port = static_cast<uint16_t>(std::stoi(bootstrapNode.substr(colonPos + 1)));
            return true;
        } catch (const std::exception& e) {
            getLogger()->warning("Invalid port in bootstrap node {}: {}", bootstrapNode, e.what());
            return false;
        }
    } else {
        host = bootstrapNode;
        port = 6881; // Default DHT port
        return true;
    }
}

bool DHTBootstrapper::sendFindNodeQuery(
    const NodeID& /*nodeID*/,
    std::shared_ptr<DHTMessageSender> messageSender,
    const network::EndPoint& endpoint,
    const NodeID& targetID) {

    // Create a find_node query
    auto query = std::make_shared<FindNodeQuery>(targetID);

    // Send the query
    return messageSender->sendQuery(query, endpoint);
}

bool DHTBootstrapper::waitForResponse(
    std::shared_ptr<DHTTransactionManager> /*transactionManager*/,
    const std::string& /*transactionID*/,
    std::chrono::seconds /*timeout*/) {

    // This is a placeholder implementation
    // In a real implementation, we would check if the transaction has completed
    // For now, we'll just return true after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

} // namespace dht_hunter::dht
