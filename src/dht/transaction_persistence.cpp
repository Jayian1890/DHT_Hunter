#include "dht_hunter/dht/transaction_persistence.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>

DEFINE_COMPONENT_LOGGER("DHT", "TransactionPersistence")

namespace dht_hunter::dht {

std::string transactionTypeToString(TransactionType type) {
    switch (type) {
        case TransactionType::Ping:
            return "ping";
        case TransactionType::FindNode:
            return "find_node";
        case TransactionType::GetPeers:
            return "get_peers";
        case TransactionType::AnnouncePeer:
            return "announce_peer";
        case TransactionType::Unknown:
        default:
            return "unknown";
    }
}

TransactionType stringToTransactionType(const std::string& type) {
    if (type == "ping") {
        return TransactionType::Ping;
    } else if (type == "find_node") {
        return TransactionType::FindNode;
    } else if (type == "get_peers") {
        return TransactionType::GetPeers;
    } else if (type == "announce_peer") {
        return TransactionType::AnnouncePeer;
    } else {
        return TransactionType::Unknown;
    }
}

bool saveTransactions(const std::string& filePath, const std::vector<SerializableTransaction>& transactions) {
    getLogger()->debug("Saving {} transactions to file: {}", transactions.size(), filePath);
    
    try {
        // Check if the directory exists and create it if needed
        std::string directory = filePath.substr(0, filePath.find_last_of('/'));
        if (!directory.empty()) {
            getLogger()->debug("Checking directory: {}", directory);
            try {
                if (!std::filesystem::exists(directory)) {
                    getLogger()->info("Creating directory: {}", directory);
                    if (!std::filesystem::create_directories(directory)) {
                        getLogger()->error("Failed to create directory: {}", directory);
                        return false;
                    }
                }
            } catch (const std::exception& e) {
                getLogger()->error("Exception while creating directory {}: {}", directory, e.what());
                return false;
            }
        }

        // Open the file for writing
        getLogger()->debug("Opening file for writing: {}", filePath);
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open transactions file for writing: {}", filePath);
            return false;
        }

        // Write the number of transactions
        uint32_t numTransactions = static_cast<uint32_t>(transactions.size());
        getLogger()->debug("Writing {} transactions to file", numTransactions);
        file.write(reinterpret_cast<const char*>(&numTransactions), sizeof(numTransactions));

        // Write each transaction
        for (const auto& transaction : transactions) {
            // Write the transaction ID length
            uint32_t idLength = static_cast<uint32_t>(transaction.id.size());
            file.write(reinterpret_cast<const char*>(&idLength), sizeof(idLength));
            
            // Write the transaction ID
            if (idLength > 0) {
                file.write(reinterpret_cast<const char*>(transaction.id.data()), static_cast<std::streamsize>(idLength));
            }
            
            // Write the timestamp
            file.write(reinterpret_cast<const char*>(&transaction.timestampMs), sizeof(transaction.timestampMs));
            
            // Write the transaction type
            uint8_t typeValue = static_cast<uint8_t>(transaction.type);
            file.write(reinterpret_cast<const char*>(&typeValue), sizeof(typeValue));
            
            // Write the associated data length
            uint32_t dataLength = static_cast<uint32_t>(transaction.associatedData.size());
            file.write(reinterpret_cast<const char*>(&dataLength), sizeof(dataLength));
            
            // Write the associated data
            if (dataLength > 0) {
                file.write(reinterpret_cast<const char*>(transaction.associatedData.data()), static_cast<std::streamsize>(dataLength));
            }
        }

        // Flush the file
        file.flush();
        if (!file) {
            getLogger()->error("Failed to flush transactions file: {}", filePath);
            return false;
        }

        // Close the file
        file.close();
        if (file.fail()) {
            getLogger()->error("Failed to close transactions file: {}", filePath);
            return false;
        }

        getLogger()->debug("Successfully saved {} transactions to file: {}", numTransactions, filePath);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving transactions to file: {}: {}", filePath, e.what());
        return false;
    }
}

bool loadTransactions(const std::string& filePath, std::vector<SerializableTransaction>& transactions) {
    getLogger()->debug("Loading transactions from file: {}", filePath);
    
    try {
        // Check if the file exists
        if (!std::filesystem::exists(filePath)) {
            getLogger()->info("Transactions file does not exist: {}", filePath);
            return false;
        }

        // Open the file for reading
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open transactions file for reading: {}", filePath);
            return false;
        }

        // Read the number of transactions
        uint32_t numTransactions;
        if (!file.read(reinterpret_cast<char*>(&numTransactions), sizeof(numTransactions))) {
            getLogger()->error("Failed to read number of transactions from file: {}", filePath);
            return false;
        }

        getLogger()->debug("Loading {} transactions from file", numTransactions);
        
        // Clear the output vector
        transactions.clear();
        
        // Reserve space for the transactions
        transactions.reserve(numTransactions);

        // Read each transaction
        for (uint32_t i = 0; i < numTransactions; ++i) {
            SerializableTransaction transaction;
            
            // Read the transaction ID length
            uint32_t idLength;
            if (!file.read(reinterpret_cast<char*>(&idLength), sizeof(idLength))) {
                getLogger()->error("Failed to read transaction ID length for transaction {}", i);
                return false;
            }
            
            // Read the transaction ID
            if (idLength > 0) {
                transaction.id.resize(idLength);
                if (!file.read(reinterpret_cast<char*>(transaction.id.data()), static_cast<std::streamsize>(idLength))) {
                    getLogger()->error("Failed to read transaction ID for transaction {}", i);
                    return false;
                }
            }
            
            // Read the timestamp
            if (!file.read(reinterpret_cast<char*>(&transaction.timestampMs), sizeof(transaction.timestampMs))) {
                getLogger()->error("Failed to read timestamp for transaction {}", i);
                return false;
            }
            
            // Read the transaction type
            uint8_t typeValue;
            if (!file.read(reinterpret_cast<char*>(&typeValue), sizeof(typeValue))) {
                getLogger()->error("Failed to read transaction type for transaction {}", i);
                return false;
            }
            transaction.type = static_cast<TransactionType>(typeValue);
            
            // Read the associated data length
            uint32_t dataLength;
            if (!file.read(reinterpret_cast<char*>(&dataLength), sizeof(dataLength))) {
                getLogger()->error("Failed to read associated data length for transaction {}", i);
                return false;
            }
            
            // Read the associated data
            if (dataLength > 0) {
                transaction.associatedData.resize(dataLength);
                if (!file.read(reinterpret_cast<char*>(transaction.associatedData.data()), static_cast<std::streamsize>(dataLength))) {
                    getLogger()->error("Failed to read associated data for transaction {}", i);
                    return false;
                }
            }
            
            // Add the transaction to the output vector
            transactions.push_back(transaction);
        }

        getLogger()->debug("Successfully loaded {} transactions from file: {}", transactions.size(), filePath);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading transactions from file: {}: {}", filePath, e.what());
        return false;
    }
}

} // namespace dht_hunter::dht
