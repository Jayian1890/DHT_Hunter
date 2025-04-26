#pragma once

#include "dht_hunter/network/buffer/buffer.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <cstdint>
#include <memory>

namespace dht_hunter::network {

/**
 * @class MessageBuffer
 * @brief A buffer for network messages.
 *
 * This class combines a buffer with an endpoint, representing a message
 * that was received from or will be sent to a specific endpoint.
 */
class MessageBuffer {
public:
    /**
     * @brief Default constructor. Creates an empty message buffer.
     */
    MessageBuffer();

    /**
     * @brief Constructs a message buffer with the specified buffer and endpoint.
     * @param buffer The buffer containing the message data.
     * @param endpoint The endpoint associated with the message.
     */
    MessageBuffer(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint);

    /**
     * @brief Copy constructor.
     * @param other The message buffer to copy.
     */
    MessageBuffer(const MessageBuffer& other);

    /**
     * @brief Move constructor.
     * @param other The message buffer to move.
     */
    MessageBuffer(MessageBuffer&& other) noexcept;

    /**
     * @brief Destructor.
     */
    ~MessageBuffer();

    /**
     * @brief Copy assignment operator.
     * @param other The message buffer to copy.
     * @return A reference to this message buffer.
     */
    MessageBuffer& operator=(const MessageBuffer& other);

    /**
     * @brief Move assignment operator.
     * @param other The message buffer to move.
     * @return A reference to this message buffer.
     */
    MessageBuffer& operator=(MessageBuffer&& other) noexcept;

    /**
     * @brief Gets the buffer.
     * @return A shared pointer to the buffer.
     */
    std::shared_ptr<Buffer> getBuffer() const;

    /**
     * @brief Sets the buffer.
     * @param buffer The buffer to set.
     */
    void setBuffer(std::shared_ptr<Buffer> buffer);

    /**
     * @brief Gets the endpoint.
     * @return The endpoint.
     */
    const EndPoint& getEndpoint() const;

    /**
     * @brief Sets the endpoint.
     * @param endpoint The endpoint to set.
     */
    void setEndpoint(const EndPoint& endpoint);

    /**
     * @brief Gets a pointer to the buffer data.
     * @return A pointer to the buffer data.
     */
    uint8_t* data();

    /**
     * @brief Gets a const pointer to the buffer data.
     * @return A const pointer to the buffer data.
     */
    const uint8_t* data() const;

    /**
     * @brief Gets the size of the buffer.
     * @return The size of the buffer in bytes.
     */
    size_t size() const;

    /**
     * @brief Resizes the buffer.
     * @param size The new size of the buffer in bytes.
     */
    void resize(size_t size);

    /**
     * @brief Clears the buffer.
     */
    void clear();

    /**
     * @brief Checks if the buffer is empty.
     * @return True if the buffer is empty, false otherwise.
     */
    bool empty() const;

private:
    std::shared_ptr<Buffer> m_buffer;
    EndPoint m_endpoint;
};

/**
 * @brief Creates a shared message buffer.
 * @param buffer The buffer containing the message data.
 * @param endpoint The endpoint associated with the message.
 * @return A shared pointer to the created message buffer.
 */
std::shared_ptr<MessageBuffer> createMessageBuffer(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint);

/**
 * @brief Creates a shared message buffer with a new buffer of the specified size.
 * @param size The size of the buffer in bytes.
 * @param endpoint The endpoint associated with the message.
 * @return A shared pointer to the created message buffer.
 */
std::shared_ptr<MessageBuffer> createMessageBuffer(size_t size, const EndPoint& endpoint);

} // namespace dht_hunter::network
