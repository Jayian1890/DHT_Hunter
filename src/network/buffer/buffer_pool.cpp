#include "dht_hunter/network/buffer/buffer_pool.hpp"
#include <algorithm>

namespace dht_hunter::network {

BufferPool::BufferPool(size_t bufferSize, size_t initialCount, size_t maxCount)
    : m_bufferSize(bufferSize), m_maxPoolSize(maxCount), m_totalBuffersCreated(0) {
    
    // Create the initial buffers
    for (size_t i = 0; i < initialCount; ++i) {
        m_pool.push(createBuffer());
    }
}

BufferPool::~BufferPool() {
    clear();
}

std::shared_ptr<Buffer> BufferPool::acquire() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_pool.empty()) {
        // Create a new buffer if the pool is empty
        return createBuffer();
    } else {
        // Get a buffer from the pool
        std::shared_ptr<Buffer> buffer = m_pool.front();
        m_pool.pop();
        return buffer;
    }
}

void BufferPool::release(std::shared_ptr<Buffer> buffer) {
    if (!buffer) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear the buffer
    buffer->clear();
    
    // Add the buffer back to the pool if there's room
    if (m_pool.size() < m_maxPoolSize) {
        m_pool.push(buffer);
    }
    // Otherwise, the buffer will be destroyed when the shared_ptr goes out of scope
}

size_t BufferPool::getBufferSize() const {
    return m_bufferSize;
}

size_t BufferPool::getPoolSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

size_t BufferPool::getMaxPoolSize() const {
    return m_maxPoolSize;
}

size_t BufferPool::getTotalBuffersCreated() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalBuffersCreated;
}

size_t BufferPool::getBuffersInUse() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalBuffersCreated - m_pool.size();
}

void BufferPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear the pool
    while (!m_pool.empty()) {
        m_pool.pop();
    }
    
    // Reset the counter
    m_totalBuffersCreated = 0;
}

std::shared_ptr<Buffer> BufferPool::createBuffer() {
    ++m_totalBuffersCreated;
    return std::make_shared<Buffer>(m_bufferSize);
}

BufferPool& BufferPool::getInstance(size_t bufferSize, size_t initialCount, size_t maxCount) {
    static BufferPool instance(bufferSize, initialCount, maxCount);
    return instance;
}

} // namespace dht_hunter::network
