#include "dht_hunter/network/buffer/buffer.hpp"
#include <cstring>

namespace dht_hunter::network {

Buffer::Buffer()
    : m_data() {
}

Buffer::Buffer(size_t size)
    : m_data(size) {
}

Buffer::Buffer(const void* data, size_t size)
    : m_data(size) {
    if (data && size > 0) {
        std::memcpy(m_data.data(), data, size);
    }
}

Buffer::Buffer(const Buffer& other)
    : m_data(other.m_data) {
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_data(std::move(other.m_data)) {
}

Buffer::~Buffer() {
}

Buffer& Buffer::operator=(const Buffer& other) {
    if (this != &other) {
        m_data = other.m_data;
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        m_data = std::move(other.m_data);
    }
    return *this;
}

uint8_t* Buffer::data() {
    return m_data.data();
}

const uint8_t* Buffer::data() const {
    return m_data.data();
}

size_t Buffer::size() const {
    return m_data.size();
}

void Buffer::resize(size_t size) {
    m_data.resize(size);
}

void Buffer::reserve(size_t size) {
    m_data.reserve(size);
}

void Buffer::clear() {
    m_data.clear();
}

void Buffer::append(const void* data, size_t size) {
    if (data && size > 0) {
        size_t oldSize = m_data.size();
        m_data.resize(oldSize + size);
        std::memcpy(m_data.data() + oldSize, data, size);
    }
}

void Buffer::append(const Buffer& other) {
    if (!other.empty()) {
        append(other.data(), other.size());
    }
}

void Buffer::consume(size_t size) {
    if (size >= m_data.size()) {
        m_data.clear();
    } else if (size > 0) {
        m_data.erase(m_data.begin(), m_data.begin() + size);
    }
}

bool Buffer::empty() const {
    return m_data.empty();
}

std::shared_ptr<Buffer> createBuffer(size_t size) {
    return std::make_shared<Buffer>(size);
}

std::shared_ptr<Buffer> createBuffer(const void* data, size_t size) {
    return std::make_shared<Buffer>(data, size);
}

} // namespace dht_hunter::network
