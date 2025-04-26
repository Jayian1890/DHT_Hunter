#pragma once

#include "dht_hunter/network/buffer/buffer.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>

namespace dht_hunter::network {

/**
 * @class BufferPool
 * @brief A pool of reusable buffers.
 *
 * This class provides a pool of reusable buffers, which can be acquired and released
 * as needed. This reduces memory allocations and improves performance.
 */
class BufferPool {
public:
    /**
     * @brief Constructs a buffer pool.
     * @param bufferSize The size of each buffer in bytes.
     * @param initialCount The initial number of buffers in the pool.
     * @param maxCount The maximum number of buffers in the pool.
     */
    BufferPool(size_t bufferSize, size_t initialCount = 16, size_t maxCount = 1024);

    /**
     * @brief Destructor.
     */
    ~BufferPool();

    /**
     * @brief Acquires a buffer from the pool.
     * @return A shared pointer to the acquired buffer.
     */
    std::shared_ptr<Buffer> acquire();

    /**
     * @brief Releases a buffer back to the pool.
     * @param buffer The buffer to release.
     */
    void release(std::shared_ptr<Buffer> buffer);

    /**
     * @brief Gets the size of each buffer in the pool.
     * @return The size of each buffer in bytes.
     */
    size_t getBufferSize() const;

    /**
     * @brief Gets the number of buffers currently in the pool.
     * @return The number of buffers in the pool.
     */
    size_t getPoolSize() const;

    /**
     * @brief Gets the maximum number of buffers allowed in the pool.
     * @return The maximum number of buffers.
     */
    size_t getMaxPoolSize() const;

    /**
     * @brief Gets the total number of buffers created by the pool.
     * @return The total number of buffers created.
     */
    size_t getTotalBuffersCreated() const;

    /**
     * @brief Gets the number of buffers currently in use.
     * @return The number of buffers in use.
     */
    size_t getBuffersInUse() const;

    /**
     * @brief Clears the pool, releasing all buffers.
     */
    void clear();

    /**
     * @brief Gets the singleton instance of the buffer pool.
     * @param bufferSize The size of each buffer in bytes.
     * @param initialCount The initial number of buffers in the pool.
     * @param maxCount The maximum number of buffers in the pool.
     * @return The singleton instance.
     */
    static BufferPool& getInstance(size_t bufferSize = 4096, size_t initialCount = 16, size_t maxCount = 1024);

private:
    /**
     * @brief Creates a new buffer.
     * @return A shared pointer to the created buffer.
     */
    std::shared_ptr<Buffer> createBuffer();

    size_t m_bufferSize;
    size_t m_maxPoolSize;
    size_t m_totalBuffersCreated;
    std::queue<std::shared_ptr<Buffer>> m_pool;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
