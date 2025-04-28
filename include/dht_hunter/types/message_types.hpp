#pragma once

#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include <string>
#include <memory>
#include <optional>
#include <vector>

namespace dht_hunter::types {

/**
 * @brief Enum for the different types of DHT messages
 */
enum class MessageType {
    Query,
    Response,
    Error
};

/**
 * @brief Enum for the different types of DHT queries
 */
enum class QueryMethod {
    Ping,
    FindNode,
    GetPeers,
    AnnouncePeer
};

/**
 * @brief DHT error codes as defined in BEP-5
 */
enum class ErrorCode {
    // Generic errors (200-299)
    GenericError = 201,      // Generic error
    ServerError = 202,       // Server error
    ProtocolError = 203,     // Protocol error, such as a malformed packet, invalid arguments, or bad token
    MethodUnknown = 204,     // Method unknown
    
    // Specific errors (300-399)
    InvalidToken = 301,      // Invalid token
    InvalidNodeID = 302,     // Invalid node ID
    InvalidInfoHash = 303,   // Invalid info hash
    InvalidPort = 304,       // Invalid port
    InvalidIP = 305,         // Invalid IP address
    
    // Implementation-specific errors (400-499)
    RateLimited = 401,       // Rate limited
    Unauthorized = 402,      // Unauthorized
    NotImplemented = 403,    // Not implemented
    
    // Unknown error
    Unknown = 999            // Unknown error
};

/**
 * @brief BitTorrent message IDs
 */
enum class BTMessageID : uint8_t {
    CHOKE = 0,
    UNCHOKE = 1,
    INTERESTED = 2,
    NOT_INTERESTED = 3,
    HAVE = 4,
    BITFIELD = 5,
    REQUEST = 6,
    PIECE = 7,
    CANCEL = 8,
    PORT = 9  // DHT port message
};

} // namespace dht_hunter::types
