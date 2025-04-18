#pragma once

#include "dht_hunter/network/network_address.hpp"

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <random>

namespace dht_hunter::dht {

/**
 * @brief Node ID type (20 bytes)
 */
using NodeID = std::array<uint8_t, 20>;

/**
 * @brief Info hash type (20 bytes)
 */
using InfoHash = std::array<uint8_t, 20>;

/**
 * @brief Transaction ID type (variable length, usually 2-4 bytes)
 */
using TransactionID = std::vector<uint8_t>;

/**
 * @brief Compact node info (26 bytes: 20 bytes node ID + 6 bytes endpoint)
 */
struct CompactNodeInfo {
    NodeID id;
    network::EndPoint endpoint;
};

/**
 * @brief DHT message type
 */
enum class MessageType {
    Query,
    Response,
    Error
};

/**
 * @brief DHT query method
 */
enum class QueryMethod {
    Ping,
    FindNode,
    GetPeers,
    AnnouncePeer,
    Unknown
};

/**
 * @brief Converts a query method to a string
 * @param method The query method
 * @return The string representation of the query method
 */
std::string queryMethodToString(QueryMethod method);

/**
 * @brief Converts a string to a query method
 * @param method The string representation of the query method
 * @return The query method
 */
QueryMethod stringToQueryMethod(const std::string& method);

/**
 * @brief Generates a random node ID
 * @return A random node ID
 */
NodeID generateRandomNodeID();

/**
 * @brief Generates a random transaction ID
 * @param length The length of the transaction ID (default: 2)
 * @return A random transaction ID
 */
TransactionID generateTransactionID(size_t length = 2);

/**
 * @brief Converts a node ID to a string
 * @param id The node ID
 * @return The string representation of the node ID
 */
std::string nodeIDToString(const NodeID& id);

/**
 * @brief Converts a string to a node ID
 * @param str The string representation of the node ID
 * @return The node ID, or std::nullopt if the string is invalid
 */
std::optional<NodeID> stringToNodeID(const std::string& str);

/**
 * @brief Converts a transaction ID to a string
 * @param id The transaction ID
 * @return The string representation of the transaction ID
 */
std::string transactionIDToString(const TransactionID& id);

/**
 * @brief Converts a string to a transaction ID
 * @param str The string representation of the transaction ID
 * @return The transaction ID
 */
TransactionID stringToTransactionID(const std::string& str);

/**
 * @brief Parses compact node info from a string
 * @param data The compact node info string
 * @return A vector of compact node info
 */
std::vector<CompactNodeInfo> parseCompactNodeInfo(const std::string& data);

/**
 * @brief Encodes compact node info to a string
 * @param nodes The compact node info
 * @return The encoded string
 */
std::string encodeCompactNodeInfo(const std::vector<CompactNodeInfo>& nodes);

} // namespace dht_hunter::dht
