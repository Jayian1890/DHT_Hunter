#include "dht_hunter/metadata/ut_metadata.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <cmath>
#include <cstring>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("UTMetadata");
}

UTMetadata::UTMetadata(uint32_t metadataSize)
    : m_metadataSize(metadataSize) {
}

void UTMetadata::setMetadataSize(uint32_t size) {
    m_metadataSize = size;
}

uint32_t UTMetadata::getMetadataSize() const {
    return m_metadataSize;
}

uint32_t UTMetadata::getNumPieces() const {
    if (m_metadataSize == 0) {
        return 0;
    }
    
    return static_cast<uint32_t>(std::ceil(static_cast<double>(m_metadataSize) / UT_METADATA_PIECE_SIZE));
}

std::vector<uint8_t> UTMetadata::createRequestMessage(uint32_t piece) const {
    // Create the message dictionary
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Set message type
    message->setInteger("msg_type", static_cast<int64_t>(UTMetadataMessageType::Request));
    
    // Set piece index
    message->setInteger("piece", piece);
    
    // Encode the message
    std::string encodedMessage = bencode::BencodeEncoder::encode(*message);
    
    // Convert to byte vector
    return std::vector<uint8_t>(encodedMessage.begin(), encodedMessage.end());
}

std::vector<uint8_t> UTMetadata::createDataMessage(uint32_t piece, const uint8_t* data, uint32_t size) const {
    // Create the message dictionary
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Set message type
    message->setInteger("msg_type", static_cast<int64_t>(UTMetadataMessageType::Data));
    
    // Set piece index
    message->setInteger("piece", piece);
    
    // Set total size
    message->setInteger("total_size", m_metadataSize);
    
    // Encode the message
    std::string encodedMessage = bencode::BencodeEncoder::encode(*message);
    
    // Create the final message with the encoded dictionary and the piece data
    std::vector<uint8_t> result;
    result.reserve(encodedMessage.size() + size);
    
    // Add the encoded dictionary
    result.insert(result.end(), encodedMessage.begin(), encodedMessage.end());
    
    // Add the piece data
    result.insert(result.end(), data, data + size);
    
    return result;
}

std::vector<uint8_t> UTMetadata::createRejectMessage(uint32_t piece) const {
    // Create the message dictionary
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Set message type
    message->setInteger("msg_type", static_cast<int64_t>(UTMetadataMessageType::Reject));
    
    // Set piece index
    message->setInteger("piece", piece);
    
    // Encode the message
    std::string encodedMessage = bencode::BencodeEncoder::encode(*message);
    
    // Convert to byte vector
    return std::vector<uint8_t>(encodedMessage.begin(), encodedMessage.end());
}

bool UTMetadata::processMessage(const uint8_t* data, size_t length,
                              const MetadataRequestCallback& requestCallback,
                              const MetadataPieceCallback& pieceCallback,
                              const MetadataRejectCallback& rejectCallback) {
    if (!data || length == 0) {
        logger->error("Invalid ut_metadata message: empty data");
        return false;
    }
    
    try {
        // Find the end of the bencoded dictionary
        size_t dictEnd = 0;
        int depth = 0;
        bool inString = false;
        size_t stringLength = 0;
        
        for (size_t i = 0; i < length; ++i) {
            char c = static_cast<char>(data[i]);
            
            if (inString) {
                if (stringLength == 0) {
                    // We're at the colon, start counting characters
                    inString = false;
                } else {
                    // We're in the string content
                    stringLength--;
                }
            } else if (c >= '0' && c <= '9') {
                if (!inString) {
                    // Start of a string length
                    inString = true;
                    stringLength = c - '0';
                }
            } else if (c == 'i' || c == 'l' || c == 'd') {
                // Start of integer, list, or dictionary
                depth++;
            } else if (c == 'e') {
                // End of integer, list, or dictionary
                depth--;
                if (depth == 0) {
                    // End of the root dictionary
                    dictEnd = i + 1;
                    break;
                }
            }
        }
        
        if (dictEnd == 0) {
            logger->error("Invalid ut_metadata message: could not find end of dictionary");
            return false;
        }
        
        // Decode the dictionary
        std::string dictStr(reinterpret_cast<const char*>(data), dictEnd);
        auto message = bencode::BencodeDecoder::decode(dictStr);
        
        if (!message || !message->isDictionary()) {
            logger->error("Invalid ut_metadata message: not a dictionary");
            return false;
        }
        
        // Get message type
        auto msgType = message->getInteger("msg_type");
        if (!msgType) {
            logger->error("Invalid ut_metadata message: missing msg_type");
            return false;
        }
        
        // Get piece index
        auto piece = message->getInteger("piece");
        if (!piece) {
            logger->error("Invalid ut_metadata message: missing piece");
            return false;
        }
        
        uint32_t pieceIndex = static_cast<uint32_t>(*piece);
        
        // Process based on message type
        switch (static_cast<UTMetadataMessageType>(*msgType)) {
            case UTMetadataMessageType::Request: {
                logger->debug("Received ut_metadata request for piece {}", pieceIndex);
                
                if (requestCallback) {
                    auto pieceData = requestCallback(pieceIndex);
                    if (!pieceData.empty()) {
                        // We have the piece, send it
                        logger->debug("Sending ut_metadata piece {}", pieceIndex);
                    } else {
                        // We don't have the piece, reject the request
                        logger->debug("Rejecting ut_metadata request for piece {}", pieceIndex);
                    }
                }
                break;
            }
            
            case UTMetadataMessageType::Data: {
                logger->debug("Received ut_metadata data for piece {}", pieceIndex);
                
                // Get total size
                auto totalSize = message->getInteger("total_size");
                if (!totalSize) {
                    logger->error("Invalid ut_metadata data message: missing total_size");
                    return false;
                }
                
                uint32_t metadataSize = static_cast<uint32_t>(*totalSize);
                
                // Update metadata size if needed
                if (m_metadataSize == 0) {
                    m_metadataSize = metadataSize;
                } else if (m_metadataSize != metadataSize) {
                    logger->warning("Metadata size mismatch: expected {}, got {}", 
                                   m_metadataSize, metadataSize);
                }
                
                // Extract the piece data
                const uint8_t* pieceData = data + dictEnd;
                uint32_t pieceSize = static_cast<uint32_t>(length - dictEnd);
                
                // Calculate expected piece size
                uint32_t expectedSize = UT_METADATA_PIECE_SIZE;
                if (pieceIndex == getNumPieces() - 1) {
                    // Last piece might be smaller
                    expectedSize = m_metadataSize % UT_METADATA_PIECE_SIZE;
                    if (expectedSize == 0) {
                        expectedSize = UT_METADATA_PIECE_SIZE;
                    }
                }
                
                if (pieceSize != expectedSize) {
                    logger->warning("Piece size mismatch for piece {}: expected {}, got {}", 
                                   pieceIndex, expectedSize, pieceSize);
                }
                
                // Notify the callback
                if (pieceCallback) {
                    pieceCallback(pieceIndex, pieceData, pieceSize, metadataSize);
                }
                break;
            }
            
            case UTMetadataMessageType::Reject: {
                logger->debug("Received ut_metadata reject for piece {}", pieceIndex);
                
                // Notify the callback
                if (rejectCallback) {
                    rejectCallback(pieceIndex);
                }
                break;
            }
            
            default:
                logger->warning("Unknown ut_metadata message type: {}", *msgType);
                return false;
        }
        
        return true;
    } catch (const bencode::BencodeException& e) {
        logger->error("Failed to decode ut_metadata message: {}", e.what());
        return false;
    }
}

} // namespace dht_hunter::metadata
