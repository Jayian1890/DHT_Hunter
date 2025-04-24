#include "dht_hunter/network/buffer_pool.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <sstream>
#include <algorithm>

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.BufferPool");
}

//
// Buffer implementation
//

Buffer::Buffer(size_t size)
    : m_data(size),
      m_usedSize(0) {
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

void Buffer::resize(size_t newSize) {
    m_data.resize(newSize);
    m_usedSize = std::min(m_usedSize, newSize);
}

void Buffer::clear() {
    m_usedSize = 0;
}

void Buffer::setUsedSize(size_t usedSize) {
    m_usedSize = std::min(usedSize, m_data.size());
}

size_t Buffer::getUsedSize() const {
    return m_usedSize;
}

//
// BufferPool implementation
//

BufferPool::BufferDeleter::BufferDeleter(BufferPool* pool)
    : m_pool(pool) {
}

void BufferPool::BufferDeleter::operator()(Buffer* buffer) {
    if (m_pool) {
        // Clear the buffer before returning it to the pool
        buffer->clear();
        
        // Return the buffer to the pool
        m_pool->returnBuffer(std::shared_ptr<Buffer>(buffer, BufferDeleter(nullptr)));
    } else {
        // If the pool is null, just delete the buffer
        delete buffer;
    }
}

BufferPool::BufferPool(size_t bufferSize, size_t initialBuffers, size_t maxBuffers)
    : m_bufferSize(bufferSize),
      m_maxBuffers(maxBuffers),
      m_totalBuffers(0),
      m_buffersInUse(0) {
    
    // Create initial buffers
    for (size_t i = 0; i < initialBuffers; ++i) {
        m_buffers.push(new Buffer(bufferSize));
        m_totalBuffers++;
    }
    
    logger->debug("Created buffer pool with buffer size: {}, initial buffers: {}, max buffers: {}",
                 bufferSize, initialBuffers, maxBuffers);
}

BufferPool::~BufferPool() {
    // Delete all buffers in the pool
    std::lock_guard<std::mutex> lock(m_mutex);
    
    while (!m_buffers.empty()) {
        delete m_buffers.front();
        m_buffers.pop();
    }
    
    logger->debug("Destroyed buffer pool with buffer size: {}, total buffers: {}",
                 m_bufferSize, m_totalBuffers.load());
}

std::shared_ptr<Buffer> BufferPool::getBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Buffer* buffer = nullptr;
    
    if (!m_buffers.empty()) {
        // Get a buffer from the pool
        buffer = m_buffers.front();
        m_buffers.pop();
    } else {
        // Create a new buffer
        buffer = new Buffer(m_bufferSize);
        m_totalBuffers++;
    }
    
    // Increment the buffers in use count
    m_buffersInUse++;
    
    // Return the buffer with a custom deleter that will return it to the pool
    return std::shared_ptr<Buffer>(buffer, BufferDeleter(this));
}

void BufferPool::returnBuffer(std::shared_ptr<Buffer> buffer) {
    // This method is called by the BufferDeleter when a buffer is no longer in use
    
    // Get the raw pointer from the shared_ptr
    Buffer* rawBuffer = buffer.get();
    
    // Decrement the buffers in use count
    m_buffersInUse--;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we should keep this buffer in the pool
    if (m_maxBuffers == 0 || m_buffers.size() < m_maxBuffers) {
        // Add the buffer back to the pool
        m_buffers.push(rawBuffer);
    } else {
        // Delete the buffer
        delete rawBuffer;
        m_totalBuffers--;
    }
}

std::shared_ptr<Buffer> BufferPool::createBuffer() {
    // Create a new buffer with the pool's buffer size
    Buffer* buffer = new Buffer(m_bufferSize);
    
    // Increment the total buffers count
    m_totalBuffers++;
    
    // Return the buffer with a custom deleter that will return it to the pool
    return std::shared_ptr<Buffer>(buffer, BufferDeleter(this));
}

size_t BufferPool::getBufferSize() const {
    return m_bufferSize;
}

size_t BufferPool::getPoolSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_buffers.size();
}

size_t BufferPool::getTotalBuffers() const {
    return m_totalBuffers.load();
}

size_t BufferPool::getBuffersInUse() const {
    return m_buffersInUse.load();
}

void BufferPool::setMaxBuffers(size_t maxBuffers) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_maxBuffers = maxBuffers;
    
    // If the new max is less than the current pool size, remove excess buffers
    if (maxBuffers > 0 && m_buffers.size() > maxBuffers) {
        size_t excess = m_buffers.size() - maxBuffers;
        
        for (size_t i = 0; i < excess; ++i) {
            delete m_buffers.front();
            m_buffers.pop();
            m_totalBuffers--;
        }
    }
}

size_t BufferPool::getMaxBuffers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxBuffers;
}

//
// MessageBuffer implementation
//

MessageBuffer::MessageBuffer(std::shared_ptr<BufferPool> bufferPool)
    : m_buffer(bufferPool->getBuffer()),
      m_readPos(0),
      m_writePos(0) {
}

MessageBuffer::MessageBuffer(std::shared_ptr<Buffer> buffer)
    : m_buffer(buffer),
      m_readPos(0),
      m_writePos(buffer->getUsedSize()) {
}

uint8_t* MessageBuffer::data() {
    return m_buffer->data();
}

const uint8_t* MessageBuffer::data() const {
    return m_buffer->data();
}

size_t MessageBuffer::size() const {
    return m_buffer->size();
}

size_t MessageBuffer::getUsedSize() const {
    return m_buffer->getUsedSize();
}

void MessageBuffer::setUsedSize(size_t usedSize) {
    m_buffer->setUsedSize(usedSize);
    m_writePos = std::min(m_writePos, usedSize);
}

void MessageBuffer::clear() {
    m_buffer->clear();
    m_readPos = 0;
    m_writePos = 0;
}

void MessageBuffer::resize(size_t newSize) {
    m_buffer->resize(newSize);
    m_readPos = std::min(m_readPos, newSize);
    m_writePos = std::min(m_writePos, newSize);
}

size_t MessageBuffer::getReadPos() const {
    return m_readPos;
}

void MessageBuffer::setReadPos(size_t pos) {
    m_readPos = std::min(pos, m_buffer->size());
}

size_t MessageBuffer::getWritePos() const {
    return m_writePos;
}

void MessageBuffer::setWritePos(size_t pos) {
    m_writePos = std::min(pos, m_buffer->size());
    m_buffer->setUsedSize(std::max(m_buffer->getUsedSize(), m_writePos));
}

size_t MessageBuffer::getBytesAvailable() const {
    return m_writePos - m_readPos;
}

size_t MessageBuffer::getSpaceAvailable() const {
    return m_buffer->size() - m_writePos;
}

size_t MessageBuffer::read(void* dest, size_t length) {
    // Check if we have enough data to read
    size_t available = getBytesAvailable();
    if (length > available) {
        length = available;
    }
    
    if (length == 0) {
        return 0;
    }
    
    // Copy the data
    std::memcpy(dest, m_buffer->data() + m_readPos, length);
    
    // Update the read position
    m_readPos += length;
    
    return length;
}

size_t MessageBuffer::write(const void* src, size_t length) {
    // Check if we have enough space to write
    size_t available = getSpaceAvailable();
    if (length > available) {
        // Resize the buffer to accommodate the data
        resize(m_buffer->size() + (length - available));
    }
    
    // Copy the data
    std::memcpy(m_buffer->data() + m_writePos, src, length);
    
    // Update the write position and used size
    m_writePos += length;
    m_buffer->setUsedSize(std::max(m_buffer->getUsedSize(), m_writePos));
    
    return length;
}

std::shared_ptr<Buffer> MessageBuffer::getBuffer() const {
    return m_buffer;
}

//
// BufferPoolManager implementation
//

BufferPoolManager& BufferPoolManager::getInstance() {
    static BufferPoolManager instance;
    return instance;
}

std::shared_ptr<BufferPool> BufferPoolManager::getPool(size_t bufferSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we already have a pool for this buffer size
    auto it = m_pools.find(bufferSize);
    if (it != m_pools.end()) {
        return it->second;
    }
    
    // Create a new pool
    auto pool = std::make_shared<BufferPool>(bufferSize);
    m_pools[bufferSize] = pool;
    
    return pool;
}

std::shared_ptr<MessageBuffer> BufferPoolManager::getMessageBuffer(size_t bufferSize) {
    // Get a pool for this buffer size
    auto pool = getPool(bufferSize);
    
    // Create a message buffer using the pool
    return std::make_shared<MessageBuffer>(pool);
}

void BufferPoolManager::setMaxBuffers(size_t bufferSize, size_t maxBuffers) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we have a pool for this buffer size
    auto it = m_pools.find(bufferSize);
    if (it != m_pools.end()) {
        it->second->setMaxBuffers(maxBuffers);
    }
}

std::string BufferPoolManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Buffer Pool Manager Statistics:" << std::endl;
    ss << "Number of pools: " << m_pools.size() << std::endl;
    
    for (const auto& [bufferSize, pool] : m_pools) {
        ss << "Pool with buffer size " << bufferSize << ":" << std::endl;
        ss << "  Total buffers: " << pool->getTotalBuffers() << std::endl;
        ss << "  Buffers in use: " << pool->getBuffersInUse() << std::endl;
        ss << "  Buffers in pool: " << pool->getPoolSize() << std::endl;
        ss << "  Max buffers: " << pool->getMaxBuffers() << std::endl;
    }
    
    return ss.str();
}

} // namespace dht_hunter::network
