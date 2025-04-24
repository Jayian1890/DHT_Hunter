#pragma once

#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/network/network_address_hash.hpp"
#include "dht_hunter/network/buffer_pool.hpp"

#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>

namespace dht_hunter::network {

/**
 * @brief Configuration for the UDP message batcher
 */
struct UDPBatcherConfig {
    size_t maxBatchSize = 1400;                             ///< Maximum size of a batch in bytes
    std::chrono::milliseconds maxBatchDelay{10};            ///< Maximum delay before sending a batch
    size_t maxMessagesPerBatch = 50;                        ///< Maximum number of messages per batch
    bool enabled = true;                                    ///< Whether batching is enabled
};

/**
 * @brief A message to be sent via UDP
 */
struct UDPMessage {
    std::vector<uint8_t> data;                              ///< The message data
    EndPoint endpoint;                                      ///< The destination endpoint
    std::function<void(bool)> callback;                     ///< Callback to invoke when the message is sent
};

/**
 * @brief Batches outgoing UDP messages to improve performance
 *
 * This class batches multiple small UDP messages to the same destination
 * into a single packet to reduce the number of system calls and improve
 * performance.
 */
class UDPMessageBatcher {
public:
    /**
     * @brief Constructs a UDP message batcher
     * @param socket The UDP socket to use for sending messages
     * @param config The batcher configuration
     */
    UDPMessageBatcher(std::shared_ptr<Socket> socket, const UDPBatcherConfig& config = UDPBatcherConfig());

    /**
     * @brief Destructor
     */
    ~UDPMessageBatcher();

    /**
     * @brief Starts the batcher
     * @return True if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the batcher
     */
    void stop();

    /**
     * @brief Checks if the batcher is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Sends a message
     * @param data The message data
     * @param endpoint The destination endpoint
     * @param callback The callback to invoke when the message is sent
     * @return True if the message was queued successfully, false otherwise
     */
    bool sendMessage(const std::vector<uint8_t>& data, const EndPoint& endpoint,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Sends a message
     * @param data The message data
     * @param length The length of the message data
     * @param endpoint The destination endpoint
     * @param callback The callback to invoke when the message is sent
     * @return True if the message was queued successfully, false otherwise
     */
    bool sendMessage(const uint8_t* data, size_t length, const EndPoint& endpoint,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Flushes all pending messages
     */
    void flush();

    /**
     * @brief Gets the number of pending messages
     * @return The number of pending messages
     */
    size_t getPendingMessageCount() const;

    /**
     * @brief Gets the configuration
     * @return The configuration
     */
    UDPBatcherConfig getConfig() const;

    /**
     * @brief Sets the configuration
     * @param config The configuration
     */
    void setConfig(const UDPBatcherConfig& config);

private:
    /**
     * @brief A batch of messages to the same destination
     */
    struct MessageBatch {
        std::vector<uint8_t> data;                          ///< The batch data
        std::vector<std::function<void(bool)>> callbacks;   ///< Callbacks for each message in the batch
        std::chrono::steady_clock::time_point creationTime; ///< When the batch was created
        size_t messageCount;                                ///< Number of messages in the batch
    };

    /**
     * @brief Processes the message queue
     */
    void processQueue();

    /**
     * @brief Thread function for processing the message queue
     */
    void processingThread();

    /**
     * @brief Sends a batch of messages
     * @param endpoint The destination endpoint
     * @param batch The batch to send
     */
    void sendBatch(const EndPoint& endpoint, MessageBatch& batch);

    std::shared_ptr<Socket> m_socket;                       ///< The UDP socket
    UDPBatcherConfig m_config;                              ///< The batcher configuration

    std::unordered_map<EndPoint, MessageBatch> m_batches;   ///< Batches by destination
    std::queue<UDPMessage> m_messageQueue;                  ///< Queue of messages to process

    std::atomic<bool> m_running;                            ///< Whether the batcher is running
    std::thread m_processingThread;                         ///< Thread for processing the message queue

    mutable std::mutex m_mutex;                             ///< Mutex for thread safety
    std::condition_variable m_condition;                    ///< Condition variable for the message queue

    std::shared_ptr<BufferPool> m_bufferPool;               ///< Buffer pool for message batches
};

} // namespace dht_hunter::network
