#include "dht_hunter/network/udp_message_batcher.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.UDPMessageBatcher");
}

UDPMessageBatcher::UDPMessageBatcher(std::shared_ptr<Socket> socket, const UDPBatcherConfig& config)
    : m_socket(socket),
      m_config(config),
      m_running(false) {
    
    // Create a buffer pool for message batches
    m_bufferPool = std::make_shared<BufferPool>(m_config.maxBatchSize, 10);
    
    logger->debug("Created UDPMessageBatcher with max batch size: {} bytes, max delay: {} ms",
                 m_config.maxBatchSize, m_config.maxBatchDelay.count());
}

UDPMessageBatcher::~UDPMessageBatcher() {
    stop();
}

bool UDPMessageBatcher::start() {
    if (m_running) {
        logger->warning("UDPMessageBatcher already running");
        return false;
    }
    
    // Check if the socket is valid
    if (!m_socket || !m_socket->isValid()) {
        logger->error("Invalid socket");
        return false;
    }
    
    // Check if the socket is a UDP socket
    if (m_socket->getType() != SocketType::UDP) {
        logger->error("Socket is not a UDP socket");
        return false;
    }
    
    m_running = true;
    
    // Start the processing thread
    m_processingThread = std::thread(&UDPMessageBatcher::processingThread, this);
    
    logger->debug("UDPMessageBatcher started");
    return true;
}

void UDPMessageBatcher::stop() {
    if (!m_running) {
        return;
    }
    
    logger->debug("Stopping UDPMessageBatcher");
    
    m_running = false;
    
    // Notify the processing thread
    m_condition.notify_one();
    
    // Wait for the processing thread to finish
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
    
    // Flush any remaining messages
    flush();
    
    logger->debug("UDPMessageBatcher stopped");
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
        logger->error("UDPMessageBatcher not running");
        if (callback) {
            callback(false);
        }
        return false;
    }
    
    // Check if the message is too large to batch
    if (length > m_config.maxBatchSize) {
        logger->debug("Message too large to batch ({} bytes), sending directly", length);
        
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
        logger->debug("Batching disabled, sending message directly");
        
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
    logger->debug("Processing thread started");
    
    while (m_running) {
        // Process the message queue
        processQueue();
        
        // Wait for more messages or a timeout
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait_for(lock, m_config.maxBatchDelay, [this] {
            return !m_running || !m_messageQueue.empty();
        });
    }
    
    logger->debug("Processing thread stopped");
}

void UDPMessageBatcher::sendBatch(const EndPoint& endpoint, MessageBatch& batch) {
    if (batch.messageCount == 0) {
        return;
    }
    
    logger->debug("Sending batch of {} messages ({} bytes) to {}",
                 batch.messageCount, batch.data.size(), endpoint.toString());
    
    // Send the batch
    int result = m_socket->sendTo(batch.data.data(), batch.data.size(), endpoint);
    bool success = (result > 0);
    
    // Invoke the callbacks
    for (auto& callback : batch.callbacks) {
        if (callback) {
            callback(success);
        }
    }
}

} // namespace dht_hunter::network
