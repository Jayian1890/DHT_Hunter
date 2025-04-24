#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace dht_hunter::metadata {

/**
 * @brief Name of the ut_metadata extension
 */
constexpr const char* UT_METADATA_EXTENSION_NAME = "ut_metadata";

/**
 * @brief Size of a metadata piece in bytes
 */
constexpr uint32_t UT_METADATA_PIECE_SIZE = 16384;

/**
 * @enum UTMetadataMessageType
 * @brief Types of ut_metadata messages
 */
enum class UTMetadataMessageType : uint8_t {
    Request = 0,  ///< Request a metadata piece
    Data = 1,     ///< Provide a metadata piece
    Reject = 2    ///< Reject a metadata request
};

/**
 * @brief Callback for metadata piece reception
 * @param piece The piece index
 * @param data The piece data
 * @param size The piece size
 * @param totalSize The total metadata size
 */
using MetadataPieceCallback = std::function<void(uint32_t piece, const uint8_t* data, uint32_t size, uint32_t totalSize)>;

/**
 * @brief Callback for metadata request
 * @param piece The piece index
 * @return The piece data, or empty vector if the piece is not available
 */
using MetadataRequestCallback = std::function<std::vector<uint8_t>(uint32_t piece)>;

/**
 * @brief Callback for metadata rejection
 * @param piece The piece index
 */
using MetadataRejectCallback = std::function<void(uint32_t piece)>;

/**
 * @class UTMetadata
 * @brief Implements the ut_metadata extension (BEP 9)
 */
class UTMetadata {
public:
    /**
     * @brief Constructor
     * @param metadataSize The size of the metadata (if known, 0 otherwise)
     */
    explicit UTMetadata(uint32_t metadataSize = 0);

    /**
     * @brief Sets the metadata size
     * @param size The metadata size
     */
    void setMetadataSize(uint32_t size);

    /**
     * @brief Gets the metadata size
     * @return The metadata size
     */
    uint32_t getMetadataSize() const;

    /**
     * @brief Calculates the number of pieces for the metadata
     * @return The number of pieces
     */
    uint32_t getNumPieces() const;

    /**
     * @brief Creates a metadata request message
     * @param piece The piece index to request
     * @return The serialized request message
     */
    std::vector<uint8_t> createRequestMessage(uint32_t piece) const;

    /**
     * @brief Creates a metadata data message
     * @param piece The piece index
     * @param data The piece data
     * @param size The piece size
     * @return The serialized data message
     */
    std::vector<uint8_t> createDataMessage(uint32_t piece, const uint8_t* data, uint32_t size) const;

    /**
     * @brief Creates a metadata reject message
     * @param piece The piece index to reject
     * @return The serialized reject message
     */
    std::vector<uint8_t> createRejectMessage(uint32_t piece) const;

    /**
     * @brief Processes a received ut_metadata message
     * @param data The message data
     * @param length The message length
     * @param requestCallback Callback for metadata requests
     * @param pieceCallback Callback for metadata pieces
     * @param rejectCallback Callback for metadata rejections
     * @return True if the message was processed successfully, false otherwise
     */
    bool processMessage(const uint8_t* data, size_t length,
                       const MetadataRequestCallback& requestCallback,
                       const MetadataPieceCallback& pieceCallback,
                       const MetadataRejectCallback& rejectCallback);

private:
    uint32_t m_metadataSize;  ///< Size of the metadata
};

} // namespace dht_hunter::metadata
