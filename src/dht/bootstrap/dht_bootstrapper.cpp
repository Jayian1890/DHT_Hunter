#include "dht_hunter/dht/bootstrap/dht_bootstrapper.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/network/dht_message_sender.hpp"
#include "dht_hunter/dht/network/dht_message_handler.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/hash.hpp"

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
    : m_config(config), m_threadPool(std::make_shared<util::ThreadPool>(config.maxParallelBootstraps)) {
    getLogger()->info("DHTBootstrapper created with {} bootstrap nodes", config.bootstrapNodes.size());
}

DHTBootstrapper::~DHTBootstrapper() {
    cancel();
}

void DHTBootstrapper::setConfig(const DHTBootstrapperConfig& config) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_config = config;
    getLogger()->info("DHTBootstrapper config updated with {} bootstrap nodes", config.bootstrapNodes.size());
}

const DHTBootstrapperConfig& DHTBootstrapper::getConfig() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_config;
}

BootstrapResult DHTBootstrapper::bootstrap(std::shared_ptr<DHTNode> node) {
    if (!node) {
        getLogger()->error("Cannot bootstrap: DHT node is null");
        return BootstrapResult{};
    }

    BootstrapGuard guard(m_bootstrapping);
    if (!guard.acquired()) {
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
                result.errorCode = BootstrapResult::ErrorCode::DNSResolutionFailed;
                result.errorMessage = "Failed to resolve hostname: " + host;
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
                    result.errorCode = BootstrapResult::ErrorCode::Timeout;
                    result.errorMessage = "Bootstrap process timed out after " +
                        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count()) + " seconds";
                    break;
                }

                // Attempt to bootstrap with this endpoint
                bool success = bootstrapWithEndpoint(node, endpoint, result);
                if (success) {
                    // Node was bootstrapped successfully
                    break;
                } else if (m_cancelled.load()) {
                    result.errorCode = BootstrapResult::ErrorCode::Cancelled;
                    result.errorMessage = "Bootstrap process was cancelled";
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

    BootstrapGuard guard(m_bootstrapping);
    if (!guard.acquired()) {
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
        result.errorCode = BootstrapResult::ErrorCode::InternalError;
        result.errorMessage = "Exception during bootstrap process: " + std::string(e.what());
    }

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

    // Use the thread pool to perform the DNS resolution with a timeout
    auto future = m_threadPool->enqueue([this, host, port]() {
        try {
            // Use the resolveEndpoint method to directly get endpoints
            return network::AddressResolver::resolveEndpoint(host, port);
        } catch (const std::exception& e) {
            getLogger()->warning("Exception during DNS resolution: {}", e.what());
            return std::vector<network::EndPoint>{};
        }
    });

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

    // Implement retry logic
    for (int attempt = 0; attempt < m_config.maxRetries; ++attempt) {
        // If this is a retry, log it and wait before trying again
        if (attempt > 0) {
            getLogger()->debug("Retrying bootstrap with {} (attempt {}/{})",
                endpoint.toString(), attempt + 1, m_config.maxRetries);
            std::this_thread::sleep_for(m_config.retryDelay);
        }

        // Use the thread pool to perform the bootstrap attempt with a timeout
        auto future = m_threadPool->enqueue([node, endpoint, this]() {
            try {
                return node->bootstrap(endpoint);
            } catch (const std::exception& e) {
                getLogger()->warning("Exception during bootstrap attempt: {}", e.what());
                return false;
            }
        });

        // Wait for the bootstrap attempt to complete or timeout
        auto status = future.wait_for(m_config.bootstrapAttemptTimeout);
        bool success = false;

        if (status == std::future_status::ready) {
            success = future.get();
            if (success) {
                // Update the result
                getLogger()->info("Successfully bootstrapped with {}", endpoint.toString());
                result.successfulAttempts++;
                result.successfulEndpoints.push_back(endpoint);
                return true;
            } else {
                // Only set error code if this is the last attempt
                if (attempt == m_config.maxRetries - 1) {
                    result.errorCode = BootstrapResult::ErrorCode::ConnectionFailed;
                    result.errorMessage = "Failed to connect to bootstrap node: " + endpoint.toString();
                }
            }
        } else {
            getLogger()->warning("Bootstrap attempt timed out for {}", endpoint.toString());
            // Only set error code if this is the last attempt
            if (attempt == m_config.maxRetries - 1) {
                result.errorCode = BootstrapResult::ErrorCode::Timeout;
                result.errorMessage = "Bootstrap attempt timed out for " + endpoint.toString();
            }
        }

        // Check if we've been cancelled
        if (m_cancelled.load()) {
            getLogger()->warning("Bootstrap process cancelled during retry");
            result.errorCode = BootstrapResult::ErrorCode::Cancelled;
            result.errorMessage = "Bootstrap process was cancelled";
            break;
        }
    }

    // If we get here, all attempts failed
    getLogger()->debug("Failed to bootstrap with {} after {} attempts", endpoint.toString(), m_config.maxRetries);
    result.failedEndpoints.push_back(endpoint);
    return false;
}

bool DHTBootstrapper::bootstrapWithEndpointUsingComponents(
    const NodeID& nodeID,
    std::shared_ptr<DHTMessageSender> messageSender,
    std::shared_ptr<DHTMessageHandler> /*messageHandler*/,
    std::shared_ptr<DHTTransactionManager> transactionManager,
    const network::EndPoint& endpoint,
    BootstrapResult& result) {

    getLogger()->debug("Attempting to bootstrap with {} using components", endpoint.toString());
    result.totalAttempts++;

    // Create a find_node query
    auto query = std::make_shared<FindNodeQuery>("", nodeID, nodeID);

    // Use a promise and condition variable to wait for the response
    std::promise<bool> responsePromise;
    std::mutex responseMutex;
    std::condition_variable responseCV;
    bool responseReceived = false;
    bool success = false;

    // Set up response, error, and timeout callbacks
    ResponseCallback responseCallback = [&](const std::shared_ptr<ResponseMessage>& response) {
        // Process the response
        auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
        if (findNodeResponse) {
            // Process the nodes
            const auto& nodes = findNodeResponse->getNodes();
            getLogger()->debug("Received {} nodes from bootstrap node {}", nodes.size(), endpoint.toString());
            success = !nodes.empty();
        }

        // Signal that we've received a response
        std::lock_guard<std::mutex> lock(responseMutex);
        responseReceived = true;
        responseCV.notify_one();
    };

    ErrorCallback errorCallback = [&](const std::shared_ptr<ErrorMessage>& error) {
        getLogger()->warning("Received error from bootstrap node: {} ({})",
            error->getMessage(), error->getCode());

        // Signal that we've received a response
        std::lock_guard<std::mutex> lock(responseMutex);
        responseReceived = true;
        responseCV.notify_one();
    };

    TimeoutCallback timeoutCallback = [&]() {
        getLogger()->warning("Bootstrap request to {} timed out", endpoint.toString());

        // Signal that we've received a response
        std::lock_guard<std::mutex> lock(responseMutex);
        responseReceived = true;
        responseCV.notify_one();
    };

    // Create a transaction with the callbacks
    std::string transactionID = transactionManager->createTransaction(
        query, endpoint, responseCallback, errorCallback, timeoutCallback);

    if (transactionID.empty()) {
        getLogger()->error("Failed to create transaction for bootstrap query to {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
        result.errorCode = BootstrapResult::ErrorCode::InternalError;
        result.errorMessage = "Failed to create transaction for bootstrap query";
        return false;
    }

    // Send the query
    if (!messageSender->sendQuery(query, endpoint)) {
        getLogger()->debug("Failed to send find_node query to {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
        result.errorCode = BootstrapResult::ErrorCode::ConnectionFailed;
        result.errorMessage = "Failed to send find_node query to " + endpoint.toString();
        return false;
    }

    // Wait for a response with a timeout
    {
        std::unique_lock<std::mutex> lock(responseMutex);
        if (!responseCV.wait_for(lock, m_config.bootstrapAttemptTimeout, [&]() { return responseReceived; })) {
            getLogger()->warning("Timed out waiting for bootstrap response from {}", endpoint.toString());
            result.failedEndpoints.push_back(endpoint);
            result.errorCode = BootstrapResult::ErrorCode::Timeout;
            result.errorMessage = "Timed out waiting for bootstrap response from " + endpoint.toString();
            return false;
        }
    }

    // Update the result
    if (success) {
        getLogger()->info("Successfully bootstrapped with {}", endpoint.toString());
        result.successfulAttempts++;
        result.successfulEndpoints.push_back(endpoint);
    } else {
        getLogger()->debug("Failed to bootstrap with {}", endpoint.toString());
        result.failedEndpoints.push_back(endpoint);
        result.errorCode = BootstrapResult::ErrorCode::InvalidResponse;
        result.errorMessage = "Received invalid or empty response from " + endpoint.toString();
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
    const NodeID& nodeID,
    std::shared_ptr<DHTMessageSender> messageSender,
    const network::EndPoint& endpoint,
    const NodeID& targetID) {

    // Create a find_node query with our node ID and the target ID
    // The target ID is typically our own node ID to find nodes close to us
    auto query = std::make_shared<FindNodeQuery>("", nodeID, targetID);

    // Log the query
    getLogger()->debug("Sending find_node query to {} with target ID {}",
        endpoint.toString(), util::bytesToHex(targetID.data(), targetID.size()));

    // Send the query using the message sender
    return messageSender->sendQuery(query, endpoint);
}

bool DHTBootstrapper::waitForResponse(
    std::shared_ptr<DHTTransactionManager> transactionManager,
    const std::string& transactionID,
    std::chrono::seconds timeout) {

    // Wait for the transaction to complete or timeout
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < timeout) {
        // Check if the transaction has completed (been removed from the manager)
        if (!transactionManager->hasTransaction(transactionID)) {
            getLogger()->debug("Transaction {} completed successfully", transactionID);
            return true; // Transaction completed
        }

        // Sleep a bit to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    getLogger()->warning("Transaction {} timed out", transactionID);
    return false; // Timed out
}

} // namespace dht_hunter::dht
