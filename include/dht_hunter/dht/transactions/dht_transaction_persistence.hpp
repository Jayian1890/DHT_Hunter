#pragma once

#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <string>

namespace dht_hunter::dht {

/**
 * @brief Serializes a transaction to a bencode dictionary
 * @param transaction The transaction to serialize
 * @return The serialized transaction
 */
bencode::BencodeDict serializeTransaction(const Transaction& transaction);

/**
 * @brief Deserializes a transaction from a bencode dictionary
 * @param dict The bencode dictionary
 * @return The deserialized transaction, or nullptr if deserialization failed
 */
std::shared_ptr<SerializedTransaction> deserializeTransaction(const bencode::BencodeDict& dict);

/**
 * @brief Saves transactions to a file
 * @param transactions The transactions to save
 * @param filePath The path to the file
 * @return True if the transactions were saved successfully, false otherwise
 */
bool saveTransactionsToFile(const std::unordered_map<std::string, std::shared_ptr<Transaction>>& transactions, const std::string& filePath);

/**
 * @brief Loads transactions from a file
 * @param filePath The path to the file
 * @return The loaded transactions, or an empty map if loading failed
 */
std::unordered_map<std::string, std::shared_ptr<SerializedTransaction>> loadTransactionsFromFile(const std::string& filePath);

} // namespace dht_hunter::dht
