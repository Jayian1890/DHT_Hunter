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
 * @class UDPReliability
 * @brief Provides reliability for UDP communication.
 *
 * This class implements reliability mechanisms for UDP communication,
 * including acknowledgments, retransmissions, and timeouts.
 */
class UDPReliability {
public:
    /**
     * @brief Callback for message delivery.
     * @param success True if the message was delivered, false otherwise.
     * @param messageId The ID of the message.
     */
    using DeliveryCallback = std::function<void(bool success, uint32_t messageId)>;

    /**
     * @brief Callback for message reception.
     * @param buffer The received message buffer.
     * @param endpoint The endpoint that sent the message.
     * @param messageId The ID of the message.
     */
    using ReceiveCallback = std::function<void(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, uint32_t messageId)>;

    /**
     * @brief Constructs a UDP reliability layer.
     * @param socket The UDP socket to use.
     */
    explicit UDPReliability(std::shared_ptr<UDPSocket> socket);

    /**
     * @brief Destructor.
     */
    ~UDPReliability();

    /**
     * @brief Sends a message reliably.
     * @param buffer The message buffer.
     * @param endpoint The endpoint to send to.
     * @param callback The delivery callback.
     * @return The message ID.
     */
    uint32_t sendReliable(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, DeliveryCallback callback = nullptr);

    /**
     * @brief Sets the receive callback.
     * @param callback The receive callback.
     */
    void setReceiveCallback(ReceiveCallback callback);

    /**
     * @brief Processes incoming messages.
     * @param buffer The received message buffer.
     * @param endpoint The endpoint that sent the message.
     */
    void processIncomingMessage(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint);

    /**
     * @brief Updates the reliability layer.
     * @param currentTime The current time.
     */
    void update(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /**
     * @brief Gets the maximum number of retransmissions.
     * @return The maximum number of retransmissions.
     */
    size_t getMaxRetransmissions() const;

    /**
     * @brief Sets the maximum number of retransmissions.
     * @param maxRetransmissions The maximum number of retransmissions.
     */
    void setMaxRetransmissions(size_t maxRetransmissions);

    /**
     * @brief Gets the retransmission timeout.
     * @return The retransmission timeout.
     */
    std::chrono::milliseconds getRetransmissionTimeout() const;

    /**
     * @brief Sets the retransmission timeout.
     * @param timeout The retransmission timeout.
     */
    void setRetransmissionTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Gets the acknowledgment timeout.
     * @return The acknowledgment timeout.
     */
    std::chrono::milliseconds getAcknowledgmentTimeout() const;

    /**
     * @brief Sets the acknowledgment timeout.
     * @param timeout The acknowledgment timeout.
     */
    void setAcknowledgmentTimeout(std::chrono::milliseconds timeout);

private:
    /**
     * @brief Sends an acknowledgment.
     * @param messageId The ID of the message to acknowledge.
     * @param endpoint The endpoint to send to.
     */
    void sendAcknowledgment(uint32_t messageId, const EndPoint& endpoint);

    /**
     * @brief Processes an acknowledgment.
     * @param messageId The ID of the acknowledged message.
     * @param endpoint The endpoint that sent the acknowledgment.
     */
    void processAcknowledgment(uint32_t messageId, const EndPoint& endpoint);

    /**
     * @brief Retransmits a message.
     * @param messageId The ID of the message to retransmit.
     */
    void retransmit(uint32_t messageId);

    struct OutgoingMessage {
        std::shared_ptr<Buffer> buffer;
        EndPoint endpoint;
        DeliveryCallback callback;
        std::chrono::steady_clock::time_point lastSentTime;
        size_t retransmissionCount;
    };

    struct IncomingMessage {
        std::chrono::steady_clock::time_point receivedTime;
    };

    std::shared_ptr<UDPSocket> m_socket;
    ReceiveCallback m_receiveCallback;
    std::unordered_map<uint32_t, OutgoingMessage> m_outgoingMessages;
    std::unordered_map<EndPoint, std::unordered_map<uint32_t, IncomingMessage>> m_incomingMessages;
    uint32_t m_nextMessageId;
    size_t m_maxRetransmissions;
    std::chrono::milliseconds m_retransmissionTimeout;
    std::chrono::milliseconds m_acknowledgmentTimeout;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
