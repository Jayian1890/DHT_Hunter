#include "dht_hunter/network/udp_message_batcher.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("Network", "UDPMessageBatcher")

namespace dht_hunter::network {
UDPMessageBatcher::UDPMessageBatcher(std::shared_ptr<Socket> socket, const UDPBatcherConfig& config)
    : m_socket(socket),
      m_config(config),
      m_running(false) {
    // Create a buffer pool for message batches
    m_bufferPool = std::make_shared<BufferPool>(m_config.maxBatchSize, 10);
    getLogger()->debug("Created UDPMessageBatcher with max batch size: {} bytes, max delay: {} ms",
                 m_config.maxBatchSize, m_config.maxBatchDelay.count());
}
UDPMessageBatcher::~UDPMessageBatcher() {
    stop();
}
bool UDPMessageBatcher::start() {
    if (m_running) {
        getLogger()->warning("UDPMessageBatcher already running");
        return false;
    }
    // Check if the socket is valid
    if (!m_socket || !m_socket->isValid()) {
        getLogger()->error("Invalid socket");
        return false;
    }
    // Check if the socket is a UDP socket
    if (m_socket->getType() != SocketType::UDP) {
        getLogger()->error("Socket is not a UDP socket");
        return false;
    }
    m_running = true;
    // Start the processing thread
    m_processingThread = std::thread(&UDPMessageBatcher::processingThread, this);
    getLogger()->debug("UDPMessageBatcher started");
    return true;
}
void UDPMessageBatcher::stop() {
    if (!m_running) {
        return;
    }
    getLogger()->debug("Stopping UDPMessageBatcher");
    m_running = false;
    // Notify the processing thread
    m_condition.notify_one();

    // Join the processing thread with a timeout
    if (m_processingThread.joinable()) {
        // Try to join with timeout
        const auto threadJoinTimeout = std::chrono::seconds(3);
        std::thread tempThread;
        bool joined = false;

        // Create a thread to join the processing thread with timeout
        tempThread = std::thread([this, &joined]() {
            if (m_processingThread.joinable()) {
                m_processingThread.join();
                joined = true;
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            auto joinStartTime = std::chrono::steady_clock::now();
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("Processing thread joined successfully");
            } else {
                // If we couldn't join, detach the thread to avoid blocking
                getLogger()->warning("Processing thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join or detach the original thread now
            }
        }
    }

    // Flush any remaining messages
    flush();
    getLogger()->debug("UDPMessageBatcher stopped");
}
bool UDPMessageBatcher::isRunning() const {
    return m_running;
}
bool UDPMessageBatcher::sendMessage(const std::vector<uint8_t>& data, const EndPoint& endpoint,
                                  std::function<void(bool)> callback) {
    return sendMessage(data.data(), data.size(), endpoint, callback);
}
bool UDPMessageBatcher::sendMessage(const uint8_t* data, size_t length, const EndPoint& endpoint,
                                  std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("UDPMessageBatcher not running");
        if (callback) {
            callback(false);
        }
        return false;
    }
    // Check if the message is too large to batch
    if (length > m_config.maxBatchSize) {
        getLogger()->debug("Message too large to batch ({} bytes), sending directly", length);
        // Send the message directly
        int result = m_socket->sendTo(data, length, endpoint);
        bool success = (result > 0);
        if (callback) {
            callback(success);
        }
        return success;
    }
    // Check if batching is disabled
    if (!m_config.enabled) {
        getLogger()->debug("Batching disabled, sending message directly");
        // Send the message directly
        int result = m_socket->sendTo(data, length, endpoint);
        bool success = (result > 0);
        if (callback) {
            callback(success);
        }
        return success;
    }
    // Create a message
    UDPMessage message;
    message.data.resize(length);
    std::memcpy(message.data.data(), data, length);
    message.endpoint = endpoint;
    message.callback = callback;
    // Add the message to the queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messageQueue.push(message);
    }
    // Notify the processing thread
    m_condition.notify_one();
    return true;
}
void UDPMessageBatcher::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Send all batches
    for (auto& [endpoint, batch] : m_batches) {
        if (batch.messageCount > 0) {
            sendBatch(endpoint, batch);
        }
    }
    // Clear the batches
    m_batches.clear();
}
size_t UDPMessageBatcher::getPendingMessageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = m_messageQueue.size();
    for (const auto& [endpoint, batch] : m_batches) {
        count += batch.messageCount;
    }
    return count;
}
UDPBatcherConfig UDPMessageBatcher::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}
void UDPMessageBatcher::setConfig(const UDPBatcherConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}
void UDPMessageBatcher::processQueue() {
    std::unique_lock<std::mutex> lock(m_mutex);
    // Process all messages in the queue
    while (!m_messageQueue.empty()) {
        // Get the next message
        UDPMessage message = std::move(m_messageQueue.front());
        m_messageQueue.pop();
        // Get the batch for this endpoint
        auto& batch = m_batches[message.endpoint];
        // If the batch is empty, initialize it
        if (batch.messageCount == 0) {
            batch.data.clear();
            batch.callbacks.clear();
            batch.creationTime = std::chrono::steady_clock::now();
            batch.messageCount = 0;
        }
        // Check if the message fits in the batch
        if (batch.data.size() + message.data.size() > m_config.maxBatchSize ||
            batch.messageCount >= m_config.maxMessagesPerBatch) {
            // Batch is full, send it
            sendBatch(message.endpoint, batch);
            // Reset the batch
            batch.data.clear();
            batch.callbacks.clear();
            batch.creationTime = std::chrono::steady_clock::now();
            batch.messageCount = 0;
        }
        // Add the message to the batch
        size_t offset = batch.data.size();
        batch.data.resize(offset + message.data.size());
        std::memcpy(batch.data.data() + offset, message.data.data(), message.data.size());
        batch.callbacks.push_back(message.callback);
        batch.messageCount++;
    }
    // Check for batches that have been waiting too long
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_batches.begin(); it != m_batches.end();) {
        auto& [endpoint, batch] = *it;
        if (batch.messageCount > 0 &&
            now - batch.creationTime > m_config.maxBatchDelay) {
            // Batch has been waiting too long, send it
            sendBatch(endpoint, batch);
            // Remove the batch
            it = m_batches.erase(it);
        } else {
            ++it;
        }
    }
}
void UDPMessageBatcher::processingThread() {
    getLogger()->debug("Processing thread started");

    // Use a shorter timeout to check the running flag more frequently
    const auto checkInterval = std::chrono::milliseconds(100);

    while (m_running) {
        // Process the message queue
        processQueue();

        // Wait for more messages or a timeout, but check the running flag frequently
        std::unique_lock<std::mutex> lock(m_mutex);

        // Calculate how many intervals to wait
        auto remainingTime = m_config.maxBatchDelay;
        while (m_running && remainingTime > std::chrono::milliseconds(0)) {
            // Wait for the shorter of checkInterval or remainingTime
            auto waitTime = std::min(checkInterval, std::chrono::duration_cast<std::chrono::milliseconds>(remainingTime));

            // Wait for messages or timeout
            auto status = m_condition.wait_for(lock, waitTime, [this] {
                return !m_running || !m_messageQueue.empty();
            });

            // If we got a message or should stop, break out of the wait loop
            if (status || !m_running) {
                break;
            }

            // Update remaining time
            remainingTime -= waitTime;
        }
    }

    getLogger()->debug("Processing thread stopped");
}
void UDPMessageBatcher::sendBatch(const EndPoint& endpoint, MessageBatch& batch) {
    if (batch.messageCount == 0) {
        return;
    }
    getLogger()->debug("Sending batch of {} messages ({} bytes) to {}",
                 batch.messageCount, batch.data.size(), endpoint.toString());

    // Check if socket is valid
    bool success = false;
    if (m_socket && m_socket->isValid()) {
        // Send the batch
        int result = m_socket->sendTo(batch.data.data(), batch.data.size(), endpoint);
        success = (result > 0);

        if (!success) {
            getLogger()->error("Failed to send batch to {}: {}",
                         endpoint.toString(),
                         m_socket->getErrorString(m_socket->getLastError()));
        }
    } else {
        getLogger()->error("Cannot send batch: socket is invalid or closed");
    }

    // Invoke the callbacks
    for (auto& callback : batch.callbacks) {
        if (callback) {
            callback(success);
        }
    }
}
} // namespace dht_hunter::network
