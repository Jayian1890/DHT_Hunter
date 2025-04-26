#include "dht_hunter/network/reliability/udp_reliability.hpp"
#include <algorithm>
#include <cstring>

namespace dht_hunter::network {

// Message header format:
// - 1 byte: Message type (0 = data, 1 = ack)
// - 4 bytes: Message ID
// - [Data message only] Remaining bytes: Message data

UDPReliability::UDPReliability(std::shared_ptr<UDPSocket> socket)
    : m_socket(socket), m_nextMessageId(0), m_maxRetransmissions(5),
      m_retransmissionTimeout(std::chrono::milliseconds(500)),
      m_acknowledgmentTimeout(std::chrono::milliseconds(5000)) {
}

UDPReliability::~UDPReliability() {
}

uint32_t UDPReliability::sendReliable(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, DeliveryCallback callback) {
    if (!buffer || buffer->empty() || !endpoint.isValid()) {
        if (callback) {
            callback(false, 0);
        }
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a message ID
    uint32_t messageId = m_nextMessageId++;

    // Create a new buffer with the header
    auto messageBuffer = std::make_shared<Buffer>(buffer->size() + 5);
    uint8_t* data = messageBuffer->data();

    // Set the message type (0 = data)
    data[0] = 0;

    // Set the message ID
    data[1] = static_cast<uint8_t>((messageId >> 24) & 0xFF);
    data[2] = static_cast<uint8_t>((messageId >> 16) & 0xFF);
    data[3] = static_cast<uint8_t>((messageId >> 8) & 0xFF);
    data[4] = static_cast<uint8_t>(messageId & 0xFF);

    // Copy the message data
    std::memcpy(data + 5, buffer->data(), buffer->size());

    // Store the outgoing message
    OutgoingMessage outgoingMessage;
    outgoingMessage.buffer = messageBuffer;
    outgoingMessage.endpoint = endpoint;
    outgoingMessage.callback = callback;
    outgoingMessage.lastSentTime = std::chrono::steady_clock::now();
    outgoingMessage.retransmissionCount = 0;

    m_outgoingMessages[messageId] = outgoingMessage;

    // Send the message
    m_socket->sendTo(messageBuffer->data(), static_cast<int>(messageBuffer->size()), endpoint);

    return messageId;
}

void UDPReliability::setReceiveCallback(ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_receiveCallback = callback;
}

void UDPReliability::processIncomingMessage(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint) {
    if (!buffer || buffer->size() < 5 || !endpoint.isValid()) {
        return;
    }

    const uint8_t* data = buffer->data();
    uint8_t messageType = data[0];
    uint32_t messageId = (static_cast<uint32_t>(data[1]) << 24) |
                         (static_cast<uint32_t>(data[2]) << 16) |
                         (static_cast<uint32_t>(data[3]) << 8) |
                         static_cast<uint32_t>(data[4]);

    if (messageType == 0) {
        // Data message
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if we've already received this message
        auto& incomingMessages = m_incomingMessages[endpoint];
        if (incomingMessages.find(messageId) != incomingMessages.end()) {
            // We've already received this message, just send an acknowledgment
            sendAcknowledgment(messageId, endpoint);
            return;
        }

        // Store the incoming message
        IncomingMessage incomingMessage;
        incomingMessage.receivedTime = std::chrono::steady_clock::now();
        incomingMessages[messageId] = incomingMessage;

        // Send an acknowledgment
        sendAcknowledgment(messageId, endpoint);

        // Extract the message data
        size_t dataSize = buffer->size() - 5;
        auto dataBuffer = std::make_shared<Buffer>(dataSize);
        std::memcpy(dataBuffer->data(), data + 5, dataSize);

        // Call the receive callback
        if (m_receiveCallback) {
            m_receiveCallback(dataBuffer, endpoint, messageId);
        }
    } else if (messageType == 1) {
        // Acknowledgment message
        processAcknowledgment(messageId, endpoint);
    }
}

void UDPReliability::update(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for outgoing messages that need retransmission
    for (auto it = m_outgoingMessages.begin(); it != m_outgoingMessages.end();) {
        auto& message = it->second;
        auto elapsed = currentTime - message.lastSentTime;

        if (elapsed > m_retransmissionTimeout) {
            if (message.retransmissionCount < m_maxRetransmissions) {
                // Retransmit the message
                retransmit(it->first);
            } else {
                // Message failed to deliver
                if (message.callback) {
                    message.callback(false, it->first);
                }
                it = m_outgoingMessages.erase(it);
                continue;
            }
        }

        ++it;
    }

    // Clean up old incoming messages
    for (auto it = m_incomingMessages.begin(); it != m_incomingMessages.end();) {
        auto& messages = it->second;
        for (auto msgIt = messages.begin(); msgIt != messages.end();) {
            auto elapsed = currentTime - msgIt->second.receivedTime;
            if (elapsed > m_acknowledgmentTimeout) {
                msgIt = messages.erase(msgIt);
            } else {
                ++msgIt;
            }
        }

        if (messages.empty()) {
            it = m_incomingMessages.erase(it);
        } else {
            ++it;
        }
    }
}

size_t UDPReliability::getMaxRetransmissions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxRetransmissions;
}

void UDPReliability::setMaxRetransmissions(size_t maxRetransmissions) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxRetransmissions = maxRetransmissions;
}

std::chrono::milliseconds UDPReliability::getRetransmissionTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_retransmissionTimeout;
}

void UDPReliability::setRetransmissionTimeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_retransmissionTimeout = timeout;
}

std::chrono::milliseconds UDPReliability::getAcknowledgmentTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_acknowledgmentTimeout;
}

void UDPReliability::setAcknowledgmentTimeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_acknowledgmentTimeout = timeout;
}

void UDPReliability::sendAcknowledgment(uint32_t messageId, const EndPoint& endpoint) {
    // Create a buffer for the acknowledgment
    uint8_t ackBuffer[5];

    // Set the message type (1 = ack)
    ackBuffer[0] = 1;

    // Set the message ID
    ackBuffer[1] = static_cast<uint8_t>((messageId >> 24) & 0xFF);
    ackBuffer[2] = static_cast<uint8_t>((messageId >> 16) & 0xFF);
    ackBuffer[3] = static_cast<uint8_t>((messageId >> 8) & 0xFF);
    ackBuffer[4] = static_cast<uint8_t>(messageId & 0xFF);

    // Send the acknowledgment
    m_socket->sendTo(ackBuffer, sizeof(ackBuffer), endpoint);
}

void UDPReliability::processAcknowledgment(uint32_t messageId, const EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the outgoing message
    auto it = m_outgoingMessages.find(messageId);
    if (it != m_outgoingMessages.end() && it->second.endpoint == endpoint) {
        // Message was delivered successfully
        if (it->second.callback) {
            it->second.callback(true, messageId);
        }
        m_outgoingMessages.erase(it);
    }
}

void UDPReliability::retransmit(uint32_t messageId) {
    auto it = m_outgoingMessages.find(messageId);
    if (it != m_outgoingMessages.end()) {
        auto& message = it->second;
        
        // Increment the retransmission count
        ++message.retransmissionCount;
        
        // Update the last sent time
        message.lastSentTime = std::chrono::steady_clock::now();
        
        // Resend the message
        m_socket->sendTo(message.buffer->data(), static_cast<int>(message.buffer->size()), message.endpoint);
    }
}

} // namespace dht_hunter::network
