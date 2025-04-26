#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include "dht_hunter/network/buffer/buffer.hpp"
#include "dht_hunter/network/socket/udp_socket.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @class UDPMessageBatcher
 * @brief Batches UDP messages for efficient sending.
 *
 * This class provides a mechanism for batching UDP messages to the same endpoint,
 * to reduce the number of socket operations and improve performance.
 */
class UDPMessageBatcher {
public:
    /**
     * @brief Callback for message sending.
     * @param success True if the message was sent, false otherwise.
     * @param messageId The ID of the message.
     */
    using SendCallback = std::function<void(bool success, uint32_t messageId)>;

    /**
     * @brief Constructs a UDP message batcher.
     * @param socket The UDP socket to use.
     * @param maxBatchSize The maximum size of a batch in bytes.
     * @param maxBatchDelay The maximum delay before sending a batch.
     */
    UDPMessageBatcher(std::shared_ptr<UDPSocket> socket,
                      size_t maxBatchSize = 1400,
                      std::chrono::milliseconds maxBatchDelay = std::chrono::milliseconds(50));

    /**
     * @brief Destructor.
     */
    ~UDPMessageBatcher();

    /**
     * @brief Sends a message.
     * @param buffer The message buffer.
     * @param endpoint The endpoint to send to.
     * @param callback The send callback.
     * @return The message ID.
     */
    uint32_t send(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, SendCallback callback = nullptr);

    /**
     * @brief Flushes all pending messages.
     */
    void flush();

    /**
     * @brief Updates the batcher.
     * @param currentTime The current time.
     */
    void update(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /**
     * @brief Gets the maximum batch size.
     * @return The maximum batch size in bytes.
     */
    size_t getMaxBatchSize() const;

    /**
     * @brief Sets the maximum batch size.
     * @param maxBatchSize The maximum batch size in bytes.
     */
    void setMaxBatchSize(size_t maxBatchSize);

    /**
     * @brief Gets the maximum batch delay.
     * @return The maximum batch delay.
     */
    std::chrono::milliseconds getMaxBatchDelay() const;

    /**
     * @brief Sets the maximum batch delay.
     * @param maxBatchDelay The maximum batch delay.
     */
    void setMaxBatchDelay(std::chrono::milliseconds maxBatchDelay);

    /**
     * @brief Gets the number of pending messages.
     * @return The number of pending messages.
     */
    size_t getPendingMessageCount() const;

    /**
     * @brief Gets the number of pending bytes.
     * @return The number of pending bytes.
     */
    size_t getPendingByteCount() const;

    /**
     * @brief Gets the number of batches sent.
     * @return The number of batches sent.
     */
    size_t getBatchesSent() const;

    /**
     * @brief Gets the number of messages sent.
     * @return The number of messages sent.
     */
    size_t getMessagesSent() const;

    /**
     * @brief Gets the number of bytes sent.
     * @return The number of bytes sent.
     */
    size_t getBytesSent() const;

    /**
     * @brief Resets the statistics.
     */
    void resetStatistics();

private:
    /**
     * @brief Sends a batch of messages.
     * @param endpoint The endpoint to send to.
     */
    void sendBatch(const EndPoint& endpoint);

    struct Message {
        std::shared_ptr<Buffer> buffer;
        SendCallback callback;
        uint32_t messageId;
    };

    struct Batch {
        std::vector<Message> messages;
        size_t totalSize;
        std::chrono::steady_clock::time_point firstMessageTime;
    };

    std::shared_ptr<UDPSocket> m_socket;
    size_t m_maxBatchSize;
    std::chrono::milliseconds m_maxBatchDelay;
    std::unordered_map<EndPoint, Batch> m_batches;
    uint32_t m_nextMessageId;
    size_t m_batchesSent;
    size_t m_messagesSent;
    size_t m_bytesSent;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
