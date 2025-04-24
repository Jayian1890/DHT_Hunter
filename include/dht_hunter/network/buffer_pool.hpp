#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <cstdint>
#include <functional>

namespace dht_hunter::network {

/**
 * @brief A memory buffer with reference counting
 */
class Buffer {
public:
    /**
     * @brief Constructs a buffer with the specified size
     * @param size The size of the buffer
     */
    explicit Buffer(size_t size);
    
    /**
     * @brief Destructor
     */
    ~Buffer() = default;
    
    /**
     * @brief Gets a pointer to the buffer data
     * @return A pointer to the buffer data
     */
    uint8_t* data();
    
    /**
     * @brief Gets a const pointer to the buffer data
     * @return A const pointer to the buffer data
     */
    const uint8_t* data() const;
    
    /**
     * @brief Gets the size of the buffer
     * @return The size of the buffer
     */
    size_t size() const;
    
    /**
     * @brief Resizes the buffer
     * @param newSize The new size of the buffer
     */
    void resize(size_t newSize);
    
    /**
     * @brief Clears the buffer (sets size to 0)
     */
    void clear();
    
    /**
     * @brief Sets the used size of the buffer
     * @param usedSize The used size of the buffer
     */
    void setUsedSize(size_t usedSize);
    
    /**
     * @brief Gets the used size of the buffer
     * @return The used size of the buffer
     */
    size_t getUsedSize() const;
    
private:
    std::vector<uint8_t> m_data;  ///< The buffer data
    size_t m_usedSize;            ///< The used size of the buffer
};

/**
 * @brief A pool of reusable memory buffers
 * 
 * This class provides a pool of pre-allocated memory buffers that can be
 * reused to avoid frequent memory allocations and deallocations.
 */
class BufferPool {
public:
    /**
     * @brief Constructs a buffer pool
     * @param bufferSize The size of each buffer
     * @param initialBuffers The initial number of buffers to allocate
     * @param maxBuffers The maximum number of buffers to keep in the pool (0 = unlimited)
     */
    BufferPool(size_t bufferSize, size_t initialBuffers = 10, size_t maxBuffers = 100);
    
    /**
     * @brief Destructor
     */
    ~BufferPool();
    
    /**
     * @brief Gets a buffer from the pool
     * @return A shared pointer to a buffer
     */
    std::shared_ptr<Buffer> getBuffer();
    
    /**
     * @brief Returns a buffer to the pool
     * @param buffer The buffer to return
     */
    void returnBuffer(std::shared_ptr<Buffer> buffer);
    
    /**
     * @brief Gets the size of each buffer
     * @return The size of each buffer
     */
    size_t getBufferSize() const;
    
    /**
     * @brief Gets the number of buffers currently in the pool
     * @return The number of buffers in the pool
     */
    size_t getPoolSize() const;
    
    /**
     * @brief Gets the total number of buffers allocated
     * @return The total number of buffers allocated
     */
    size_t getTotalBuffers() const;
    
    /**
     * @brief Gets the number of buffers currently in use
     * @return The number of buffers in use
     */
    size_t getBuffersInUse() const;
    
    /**
     * @brief Sets the maximum number of buffers to keep in the pool
     * @param maxBuffers The maximum number of buffers (0 = unlimited)
     */
    void setMaxBuffers(size_t maxBuffers);
    
    /**
     * @brief Gets the maximum number of buffers to keep in the pool
     * @return The maximum number of buffers (0 = unlimited)
     */
    size_t getMaxBuffers() const;
    
private:
    /**
     * @brief Creates a new buffer
     * @return A shared pointer to a new buffer
     */
    std::shared_ptr<Buffer> createBuffer();
    
    /**
     * @brief Custom deleter for buffers that returns them to the pool
     */
    class BufferDeleter {
    public:
        /**
         * @brief Constructs a buffer deleter
         * @param pool The buffer pool
         */
        explicit BufferDeleter(BufferPool* pool);
        
        /**
         * @brief Deletes the buffer (returns it to the pool)
         * @param buffer The buffer to delete
         */
        void operator()(Buffer* buffer);
        
    private:
        BufferPool* m_pool;  ///< The buffer pool
    };
    
    size_t m_bufferSize;                     ///< The size of each buffer
    size_t m_maxBuffers;                     ///< The maximum number of buffers to keep in the pool
    std::atomic<size_t> m_totalBuffers;      ///< The total number of buffers allocated
    std::atomic<size_t> m_buffersInUse;      ///< The number of buffers currently in use
    
    std::queue<Buffer*> m_buffers;           ///< The pool of available buffers
    mutable std::mutex m_mutex;              ///< Mutex for thread safety
};

/**
 * @brief A message buffer for efficient packet processing
 * 
 * This class provides a buffer for processing network messages, with
 * optimizations for reading and writing data.
 */
class MessageBuffer {
public:
    /**
     * @brief Constructs a message buffer
     * @param bufferPool The buffer pool to use
     */
    explicit MessageBuffer(std::shared_ptr<BufferPool> bufferPool);
    
    /**
     * @brief Constructs a message buffer with a specific buffer
     * @param buffer The buffer to use
     */
    explicit MessageBuffer(std::shared_ptr<Buffer> buffer);
    
    /**
     * @brief Destructor
     */
    ~MessageBuffer() = default;
    
    /**
     * @brief Gets a pointer to the buffer data
     * @return A pointer to the buffer data
     */
    uint8_t* data();
    
    /**
     * @brief Gets a const pointer to the buffer data
     * @return A const pointer to the buffer data
     */
    const uint8_t* data() const;
    
    /**
     * @brief Gets the size of the buffer
     * @return The size of the buffer
     */
    size_t size() const;
    
    /**
     * @brief Gets the used size of the buffer
     * @return The used size of the buffer
     */
    size_t getUsedSize() const;
    
    /**
     * @brief Sets the used size of the buffer
     * @param usedSize The used size of the buffer
     */
    void setUsedSize(size_t usedSize);
    
    /**
     * @brief Clears the buffer (sets used size to 0)
     */
    void clear();
    
    /**
     * @brief Resizes the buffer
     * @param newSize The new size of the buffer
     */
    void resize(size_t newSize);
    
    /**
     * @brief Gets the read position
     * @return The read position
     */
    size_t getReadPos() const;
    
    /**
     * @brief Sets the read position
     * @param pos The new read position
     */
    void setReadPos(size_t pos);
    
    /**
     * @brief Gets the write position
     * @return The write position
     */
    size_t getWritePos() const;
    
    /**
     * @brief Sets the write position
     * @param pos The new write position
     */
    void setWritePos(size_t pos);
    
    /**
     * @brief Gets the number of bytes available for reading
     * @return The number of bytes available
     */
    size_t getBytesAvailable() const;
    
    /**
     * @brief Gets the number of bytes available for writing
     * @return The number of bytes available
     */
    size_t getSpaceAvailable() const;
    
    /**
     * @brief Reads data from the buffer
     * @param dest The destination buffer
     * @param length The number of bytes to read
     * @return The number of bytes read
     */
    size_t read(void* dest, size_t length);
    
    /**
     * @brief Writes data to the buffer
     * @param src The source buffer
     * @param length The number of bytes to write
     * @return The number of bytes written
     */
    size_t write(const void* src, size_t length);
    
    /**
     * @brief Reads a value from the buffer
     * @tparam T The type of value to read
     * @return The value read
     */
    template<typename T>
    T read() {
        T value;
        read(&value, sizeof(T));
        return value;
    }
    
    /**
     * @brief Writes a value to the buffer
     * @tparam T The type of value to write
     * @param value The value to write
     * @return The number of bytes written
     */
    template<typename T>
    size_t write(const T& value) {
        return write(&value, sizeof(T));
    }
    
    /**
     * @brief Gets the underlying buffer
     * @return The underlying buffer
     */
    std::shared_ptr<Buffer> getBuffer() const;
    
private:
    std::shared_ptr<Buffer> m_buffer;  ///< The underlying buffer
    size_t m_readPos;                  ///< The current read position
    size_t m_writePos;                 ///< The current write position
};

/**
 * @brief A global buffer pool manager
 * 
 * This class provides access to global buffer pools of different sizes.
 */
class BufferPoolManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static BufferPoolManager& getInstance();
    
    /**
     * @brief Gets a buffer pool with the specified buffer size
     * @param bufferSize The size of each buffer
     * @return A shared pointer to a buffer pool
     */
    std::shared_ptr<BufferPool> getPool(size_t bufferSize);
    
    /**
     * @brief Gets a message buffer with the specified buffer size
     * @param bufferSize The size of the buffer
     * @return A shared pointer to a message buffer
     */
    std::shared_ptr<MessageBuffer> getMessageBuffer(size_t bufferSize);
    
    /**
     * @brief Sets the maximum number of buffers for a pool
     * @param bufferSize The size of each buffer in the pool
     * @param maxBuffers The maximum number of buffers (0 = unlimited)
     */
    void setMaxBuffers(size_t bufferSize, size_t maxBuffers);
    
    /**
     * @brief Gets statistics for all buffer pools
     * @return A string containing statistics
     */
    std::string getStats() const;
    
private:
    /**
     * @brief Constructs a buffer pool manager
     */
    BufferPoolManager() = default;
    
    /**
     * @brief Destructor
     */
    ~BufferPoolManager() = default;
    
    /**
     * @brief Copy constructor (deleted)
     */
    BufferPoolManager(const BufferPoolManager&) = delete;
    
    /**
     * @brief Assignment operator (deleted)
     */
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;
    
    std::unordered_map<size_t, std::shared_ptr<BufferPool>> m_pools;  ///< Buffer pools by buffer size
    mutable std::mutex m_mutex;                                       ///< Mutex for thread safety
};

} // namespace dht_hunter::network
