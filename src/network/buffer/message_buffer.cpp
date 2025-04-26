#include "dht_hunter/network/buffer/message_buffer.hpp"

namespace dht_hunter::network {

MessageBuffer::MessageBuffer()
    : m_buffer(nullptr), m_endpoint() {
}

MessageBuffer::MessageBuffer(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint)
    : m_buffer(buffer), m_endpoint(endpoint) {
}

MessageBuffer::MessageBuffer(const MessageBuffer& other)
    : m_buffer(other.m_buffer), m_endpoint(other.m_endpoint) {
}

MessageBuffer::MessageBuffer(MessageBuffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer)), m_endpoint(std::move(other.m_endpoint)) {
}

MessageBuffer::~MessageBuffer() {
}

MessageBuffer& MessageBuffer::operator=(const MessageBuffer& other) {
    if (this != &other) {
        m_buffer = other.m_buffer;
        m_endpoint = other.m_endpoint;
    }
    return *this;
}

MessageBuffer& MessageBuffer::operator=(MessageBuffer&& other) noexcept {
    if (this != &other) {
        m_buffer = std::move(other.m_buffer);
        m_endpoint = std::move(other.m_endpoint);
    }
    return *this;
}

std::shared_ptr<Buffer> MessageBuffer::getBuffer() const {
    return m_buffer;
}

void MessageBuffer::setBuffer(std::shared_ptr<Buffer> buffer) {
    m_buffer = buffer;
}

const EndPoint& MessageBuffer::getEndpoint() const {
    return m_endpoint;
}

void MessageBuffer::setEndpoint(const EndPoint& endpoint) {
    m_endpoint = endpoint;
}

uint8_t* MessageBuffer::data() {
    return m_buffer ? m_buffer->data() : nullptr;
}

const uint8_t* MessageBuffer::data() const {
    return m_buffer ? m_buffer->data() : nullptr;
}

size_t MessageBuffer::size() const {
    return m_buffer ? m_buffer->size() : 0;
}

void MessageBuffer::resize(size_t size) {
    if (m_buffer) {
        m_buffer->resize(size);
    } else if (size > 0) {
        m_buffer = std::make_shared<Buffer>(size);
    }
}

void MessageBuffer::clear() {
    if (m_buffer) {
        m_buffer->clear();
    }
}

bool MessageBuffer::empty() const {
    return !m_buffer || m_buffer->empty();
}

std::shared_ptr<MessageBuffer> createMessageBuffer(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint) {
    return std::make_shared<MessageBuffer>(buffer, endpoint);
}

std::shared_ptr<MessageBuffer> createMessageBuffer(size_t size, const EndPoint& endpoint) {
    return std::make_shared<MessageBuffer>(std::make_shared<Buffer>(size), endpoint);
}

} // namespace dht_hunter::network
