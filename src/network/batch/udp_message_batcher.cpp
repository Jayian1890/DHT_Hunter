#include "dht_hunter/network/batch/udp_message_batcher.hpp"

namespace dht_hunter::network {

UDPMessageBatcher::UDPMessageBatcher(std::shared_ptr<UDPSocket> socket, size_t maxBatchSize, std::chrono::milliseconds maxBatchDelay)
    : m_socket(socket), m_maxBatchSize(maxBatchSize), m_maxBatchDelay(maxBatchDelay), m_nextMessageId(1),
      m_batchesSent(0), m_messagesSent(0), m_bytesSent(0) {
}

UDPMessageBatcher::~UDPMessageBatcher() {
    flush();
}

uint32_t UDPMessageBatcher::send(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, SendCallback callback) {
    if (!buffer || buffer->empty() || !endpoint.isValid()) {
        if (callback) {
            callback(false, 0);
        }
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a message ID
    uint32_t messageId = m_nextMessageId++;

    // Create a message
    Message message;
    message.buffer = buffer;
    message.callback = callback;
    message.messageId = messageId;

    // Get or create the batch for this endpoint
    auto& batch = m_batches[endpoint];
    if (batch.messages.empty()) {
        batch.firstMessageTime = std::chrono::steady_clock::now();
    }

    // Add the message to the batch
    batch.messages.push_back(message);
    batch.totalSize += buffer->size();

    // Check if the batch is full
    if (batch.totalSize >= m_maxBatchSize) {
        sendBatch(endpoint);
    }

    return messageId;
}

void UDPMessageBatcher::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Send all batches
    for (auto& pair : m_batches) {
        sendBatch(pair.first);
    }

    m_batches.clear();
}

void UDPMessageBatcher::update(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for batches that have exceeded the maximum delay
    for (auto it = m_batches.begin(); it != m_batches.end();) {
        auto& batch = it->second;
        if (!batch.messages.empty() && currentTime - batch.firstMessageTime >= m_maxBatchDelay) {
            sendBatch(it->first);
            it = m_batches.erase(it);
        } else {
            ++it;
        }
    }
}

size_t UDPMessageBatcher::getMaxBatchSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxBatchSize;
}

void UDPMessageBatcher::setMaxBatchSize(size_t maxBatchSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxBatchSize = maxBatchSize;
}

std::chrono::milliseconds UDPMessageBatcher::getMaxBatchDelay() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxBatchDelay;
}

void UDPMessageBatcher::setMaxBatchDelay(std::chrono::milliseconds maxBatchDelay) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxBatchDelay = maxBatchDelay;
}

size_t UDPMessageBatcher::getPendingMessageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& pair : m_batches) {
        count += pair.second.messages.size();
    }

    return count;
}

size_t UDPMessageBatcher::getPendingByteCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& pair : m_batches) {
        count += pair.second.totalSize;
    }

    return count;
}

size_t UDPMessageBatcher::getBatchesSent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_batchesSent;
}

size_t UDPMessageBatcher::getMessagesSent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_messagesSent;
}

size_t UDPMessageBatcher::getBytesSent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bytesSent;
}

void UDPMessageBatcher::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batchesSent = 0;
    m_messagesSent = 0;
    m_bytesSent = 0;
}

void UDPMessageBatcher::sendBatch(const EndPoint& endpoint) {
    auto it = m_batches.find(endpoint);
    if (it == m_batches.end() || it->second.messages.empty()) {
        return;
    }

    auto& batch = it->second;

    // If there's only one message in the batch, send it directly
    if (batch.messages.size() == 1) {
        auto& message = batch.messages[0];
        bool success = m_socket->sendTo(message.buffer->data(), static_cast<int>(message.buffer->size()), endpoint) > 0;

        // Call the callback
        if (message.callback) {
            message.callback(success, message.messageId);
        }

        // Update statistics
        if (success) {
            ++m_batchesSent;
            ++m_messagesSent;
            m_bytesSent += message.buffer->size();
        }

        return;
    }

    // Create a buffer for the batch
    auto batchBuffer = std::make_shared<Buffer>(batch.totalSize);
    size_t offset = 0;

    // Copy all messages into the batch buffer
    for (auto& message : batch.messages) {
        std::memcpy(batchBuffer->data() + offset, message.buffer->data(), message.buffer->size());
        offset += message.buffer->size();
    }

    // Send the batch
    bool success = m_socket->sendTo(batchBuffer->data(), static_cast<int>(batchBuffer->size()), endpoint) > 0;

    // Call the callbacks
    for (auto& message : batch.messages) {
        if (message.callback) {
            message.callback(success, message.messageId);
        }
    }

    // Update statistics
    if (success) {
        ++m_batchesSent;
        m_messagesSent += batch.messages.size();
        m_bytesSent += batchBuffer->size();
    }
}

} // namespace dht_hunter::network
