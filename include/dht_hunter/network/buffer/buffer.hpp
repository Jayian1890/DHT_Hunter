#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace dht_hunter::network {

/**
 * @class Buffer
 * @brief A buffer for network data.
 *
 * This class provides a simple buffer for network data, with support for
 * resizing, copying, and moving data.
 */
class Buffer {
public:
    /**
     * @brief Default constructor. Creates an empty buffer.
     */
    Buffer();

    /**
     * @brief Constructs a buffer with the specified size.
     * @param size The size of the buffer in bytes.
     */
    explicit Buffer(size_t size);

    /**
     * @brief Constructs a buffer with the specified data.
     * @param data The data to copy into the buffer.
     * @param size The size of the data in bytes.
     */
    Buffer(const void* data, size_t size);

    /**
     * @brief Copy constructor.
     * @param other The buffer to copy.
     */
    Buffer(const Buffer& other);

    /**
     * @brief Move constructor.
     * @param other The buffer to move.
     */
    Buffer(Buffer&& other) noexcept;

    /**
     * @brief Destructor.
     */
    ~Buffer();

    /**
     * @brief Copy assignment operator.
     * @param other The buffer to copy.
     * @return A reference to this buffer.
     */
    Buffer& operator=(const Buffer& other);

    /**
     * @brief Move assignment operator.
     * @param other The buffer to move.
     * @return A reference to this buffer.
     */
    Buffer& operator=(Buffer&& other) noexcept;

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
     * @brief Reserves space in the buffer.
     * @param size The amount of space to reserve in bytes.
     */
    void reserve(size_t size);

    /**
     * @brief Clears the buffer.
     */
    void clear();

    /**
     * @brief Appends data to the buffer.
     * @param data The data to append.
     * @param size The size of the data in bytes.
     */
    void append(const void* data, size_t size);

    /**
     * @brief Appends another buffer to this buffer.
     * @param other The buffer to append.
     */
    void append(const Buffer& other);

    /**
     * @brief Removes data from the beginning of the buffer.
     * @param size The number of bytes to remove.
     */
    void consume(size_t size);

    /**
     * @brief Checks if the buffer is empty.
     * @return True if the buffer is empty, false otherwise.
     */
    bool empty() const;

private:
    std::vector<uint8_t> m_data;
};

/**
 * @brief Creates a shared buffer.
 * @param size The size of the buffer in bytes.
 * @return A shared pointer to the created buffer.
 */
std::shared_ptr<Buffer> createBuffer(size_t size);

/**
 * @brief Creates a shared buffer with the specified data.
 * @param data The data to copy into the buffer.
 * @param size The size of the data in bytes.
 * @return A shared pointer to the created buffer.
 */
std::shared_ptr<Buffer> createBuffer(const void* data, size_t size);

} // namespace dht_hunter::network
