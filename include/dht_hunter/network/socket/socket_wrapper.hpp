#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include <functional>

namespace dht_hunter::network {

/**
 * @brief A wrapper for a socket that allows sending messages
 */
class SocketWrapper {
public:
    /**
     * @brief Constructs a socket wrapper
     * @param sendCallback The callback to call when sending a message
     */
    explicit SocketWrapper(std::function<bool(const uint8_t*, size_t, const EndPoint&, std::function<void(bool)>)> sendCallback)
        : m_sendCallback(sendCallback) {}

    /**
     * @brief Sends a message
     * @param data The data to send
     * @param size The size of the data
     * @param endpoint The endpoint to send the data to
     * @param callback The callback to call when the message is sent
     * @return True if the message was queued for sending, false otherwise
     */
    bool sendTo(const uint8_t* data, size_t size, const EndPoint& endpoint, std::function<void(bool)> callback = nullptr) {
        return m_sendCallback(data, size, endpoint, callback);
    }

private:
    std::function<bool(const uint8_t*, size_t, const EndPoint&, std::function<void(bool)>)> m_sendCallback;
};

} // namespace dht_hunter::network
