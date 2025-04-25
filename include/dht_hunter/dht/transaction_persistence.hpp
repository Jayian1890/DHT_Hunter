#pragma once

#include "dht_hunter/dht/types.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace dht_hunter::dht {

/**
 * @brief Enum representing the type of transaction for persistence purposes
 */
enum class TransactionType {
    Ping,
    FindNode,
    GetPeers,
    AnnouncePeer,
    Unknown
};

/**
 * @brief Serializable representation of a transaction for persistence
 */
struct SerializableTransaction {
    TransactionID id;                      ///< Transaction ID
    uint64_t timestampMs;                  ///< Timestamp in milliseconds since epoch
    TransactionType type;                  ///< Type of transaction
    std::vector<uint8_t> associatedData;   ///< Associated data (e.g., target node ID, info hash)
};

/**
 * @brief Converts a transaction type to a string
 * @param type The transaction type
 * @return The string representation of the transaction type
 */
std::string transactionTypeToString(TransactionType type);

/**
 * @brief Converts a string to a transaction type
 * @param type The string representation of the transaction type
 * @return The transaction type
 */
TransactionType stringToTransactionType(const std::string& type);

/**
 * @brief Saves transactions to a file
 * @param filePath The path to the file
 * @param transactions The transactions to save
 * @return True if the transactions were saved successfully, false otherwise
 */
bool saveTransactions(const std::string& filePath, const std::vector<SerializableTransaction>& transactions);

/**
 * @brief Loads transactions from a file
 * @param filePath The path to the file
 * @param transactions The transactions to load into
 * @return True if the transactions were loaded successfully, false otherwise
 */
bool loadTransactions(const std::string& filePath, std::vector<SerializableTransaction>& transactions);

} // namespace dht_hunter::dht
