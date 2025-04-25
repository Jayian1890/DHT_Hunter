#include "dht_hunter/dht/transactions/dht_transaction_persistence.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

DEFINE_COMPONENT_LOGGER("DHT", "TransactionPersistence")

namespace dht_hunter::dht {

bencode::BencodeDict serializeTransaction(const Transaction& transaction) {
    bencode::BencodeDict dict;

    // Add the transaction ID
    auto idValue = std::make_shared<bencode::BencodeValue>();
    idValue->setString(transaction.getID());
    dict["id"] = idValue;

    // Add the endpoint
    auto ipValue = std::make_shared<bencode::BencodeValue>();
    ipValue->setString(transaction.getEndpoint().getAddress().toString());
    dict["ip"] = ipValue;

    auto portValue = std::make_shared<bencode::BencodeValue>();
    portValue->setInteger(static_cast<int64_t>(transaction.getEndpoint().getPort()));
    dict["port"] = portValue;

    // Add the creation time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - transaction.getCreationTime()).count();

    auto elapsedValue = std::make_shared<bencode::BencodeValue>();
    elapsedValue->setInteger(static_cast<int64_t>(elapsed));
    dict["elapsed"] = elapsedValue;

    // Add the query
    auto query = transaction.getQuery();
    if (query) {
        auto queryTypeValue = std::make_shared<bencode::BencodeValue>();
        queryTypeValue->setInteger(static_cast<int64_t>(query->getMethod()));
        dict["query_type"] = queryTypeValue;

        // Add query-specific data
        switch (query->getMethod()) {
            case QueryMethod::Ping:
                // No additional data for ping queries
                break;
            case QueryMethod::FindNode: {
                auto findNodeQuery = std::dynamic_pointer_cast<FindNodeQuery>(query);
                if (findNodeQuery) {
                    // Add the target ID
                    std::string targetIDStr(reinterpret_cast<const char*>(findNodeQuery->getTargetID().data()),
                                          findNodeQuery->getTargetID().size());
                    auto targetIDValue = std::make_shared<bencode::BencodeValue>();
                    targetIDValue->setString(targetIDStr);
                    dict["target_id"] = targetIDValue;
                }
                break;
            }
            case QueryMethod::GetPeers: {
                auto getPeersQuery = std::dynamic_pointer_cast<GetPeersQuery>(query);
                if (getPeersQuery) {
                    // Add the info hash
                    std::string infoHashStr(reinterpret_cast<const char*>(getPeersQuery->getInfoHash().data()),
                                          getPeersQuery->getInfoHash().size());
                    auto infoHashValue = std::make_shared<bencode::BencodeValue>();
                    infoHashValue->setString(infoHashStr);
                    dict["info_hash"] = infoHashValue;
                }
                break;
            }
            case QueryMethod::AnnouncePeer: {
                auto announcePeerQuery = std::dynamic_pointer_cast<AnnouncePeerQuery>(query);
                if (announcePeerQuery) {
                    // Add the info hash
                    std::string infoHashStr(reinterpret_cast<const char*>(announcePeerQuery->getInfoHash().data()),
                                          announcePeerQuery->getInfoHash().size());
                    auto infoHashValue = std::make_shared<bencode::BencodeValue>();
                    infoHashValue->setString(infoHashStr);
                    dict["info_hash"] = infoHashValue;

                    // Add the port
                    auto announcePeerPortValue = std::make_shared<bencode::BencodeValue>();
                    announcePeerPortValue->setInteger(static_cast<int64_t>(announcePeerQuery->getPort()));
                    dict["announce_port"] = announcePeerPortValue;

                    // Add the token
                    auto tokenValue = std::make_shared<bencode::BencodeValue>();
                    tokenValue->setString(announcePeerQuery->getToken());
                    dict["token"] = tokenValue;
                }
                break;
            }
            default:
                // Unknown query type
                break;
        }
    }

    return dict;
}

std::shared_ptr<SerializedTransaction> deserializeTransaction(const bencode::BencodeDict& dict) {
    auto transaction = std::make_shared<SerializedTransaction>();

    // Get the transaction ID
    auto idIt = dict.find("id");
    if (idIt == dict.end() || !idIt->second->isString()) {
        getLogger()->warning("Transaction has no ID");
        return nullptr;
    }
    transaction->id = idIt->second->getString();

    // Get the endpoint
    auto ipIt = dict.find("ip");
    if (ipIt == dict.end() || !ipIt->second->isString()) {
        getLogger()->warning("Transaction has no IP");
        return nullptr;
    }

    auto portIt = dict.find("port");
    if (portIt == dict.end() || !portIt->second->isInteger()) {
        getLogger()->warning("Transaction has no port");
        return nullptr;
    }

    transaction->endpoint = ipIt->second->getString() + ":" + std::to_string(portIt->second->getInteger());

    // Get the creation time
    auto elapsedIt = dict.find("elapsed");
    if (elapsedIt == dict.end() || !elapsedIt->second->isInteger()) {
        getLogger()->warning("Transaction has no elapsed time");
        return nullptr;
    }

    // Calculate the creation time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::seconds(elapsedIt->second->getInteger());
    auto creationTimePoint = now - elapsed;
    transaction->creationTime = creationTimePoint.time_since_epoch().count();

    // Get the query type
    auto queryTypeIt = dict.find("query_type");
    if (queryTypeIt == dict.end() || !queryTypeIt->second->isInteger()) {
        getLogger()->warning("Transaction has no query type");
        return nullptr;
    }

    transaction->queryMethod = static_cast<QueryMethod>(queryTypeIt->second->getInteger());

    // Get query-specific data
    switch (transaction->queryMethod) {
        case QueryMethod::Ping:
            // No additional data for ping queries
            break;
        case QueryMethod::FindNode: {
            // Get the target ID
            auto targetIDIt = dict.find("target_id");
            if (targetIDIt == dict.end() || !targetIDIt->second->isString()) {
                getLogger()->warning("FindNode query has no target ID");
                return nullptr;
            }

            transaction->targetID = targetIDIt->second->getString();
            break;
        }
        case QueryMethod::GetPeers: {
            // Get the info hash
            auto infoHashIt = dict.find("info_hash");
            if (infoHashIt == dict.end() || !infoHashIt->second->isString()) {
                getLogger()->warning("GetPeers query has no info hash");
                return nullptr;
            }

            transaction->infoHash = infoHashIt->second->getString();
            break;
        }
        case QueryMethod::AnnouncePeer: {
            // Get the info hash
            auto infoHashIt = dict.find("info_hash");
            if (infoHashIt == dict.end() || !infoHashIt->second->isString()) {
                getLogger()->warning("AnnouncePeer query has no info hash");
                return nullptr;
            }

            transaction->infoHash = infoHashIt->second->getString();

            // Get the port
            auto announcePortIt = dict.find("announce_port");
            if (announcePortIt == dict.end() || !announcePortIt->second->isInteger()) {
                getLogger()->warning("AnnouncePeer query has no port");
                return nullptr;
            }

            transaction->port = static_cast<uint16_t>(announcePortIt->second->getInteger());

            // Get the token
            auto tokenIt = dict.find("token");
            if (tokenIt == dict.end() || !tokenIt->second->isString()) {
                getLogger()->warning("AnnouncePeer query has no token");
                return nullptr;
            }

            transaction->token = tokenIt->second->getString();
            break;
        }
        default:
            getLogger()->warning("Unknown query type: {}", static_cast<int>(transaction->queryMethod));
            return nullptr;
    }

    // Set the state to pending
    transaction->state = TransactionState::Pending;

    return transaction;
}

bool saveTransactionsToFile(const std::unordered_map<std::string, std::shared_ptr<Transaction>>& transactions, const std::string& filePath) {
    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    // Create a bencode dictionary to store the transactions
    bencode::BencodeDict dict;

    // Add the transactions to the dictionary
    for (const auto& entry : transactions) {
        const auto& transaction = entry.second;

        // Serialize the transaction
        bencode::BencodeDict transactionDict = serializeTransaction(*transaction);

        // Add the transaction to the dictionary
        auto transactionValue = std::make_shared<bencode::BencodeValue>();
        transactionValue->setDictionary(transactionDict);
        dict[transaction->getID()] = transactionValue;
    }

    // Encode the dictionary
    std::string encoded = bencode::encode(dict);

    // Save the encoded dictionary to the file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open transactions file for writing: {}", filePath);
        return false;
    }

    file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    if (!file) {
        getLogger()->error("Failed to write transactions to file: {}", filePath);
        return false;
    }

    getLogger()->info("Transactions saved to {}", filePath);
    return true;
}

std::unordered_map<std::string, std::shared_ptr<SerializedTransaction>> loadTransactionsFromFile(const std::string& filePath) {
    std::unordered_map<std::string, std::shared_ptr<SerializedTransaction>> result;

    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        getLogger()->error("Transactions file does not exist: {}", filePath);
        return result;
    }

    // Read the file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open transactions file for reading: {}", filePath);
        return result;
    }

    // Get the file size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file contents
    std::string encoded(static_cast<size_t>(size), '\0');
    if (!file.read(&encoded[0], size)) {
        getLogger()->error("Failed to read transactions from file: {}", filePath);
        return result;
    }

    // Decode the bencode data
    bencode::BencodeParser parser;
    bencode::BencodeValue root;

    try {
        root = parser.parse(encoded);
    } catch (const std::exception& e) {
        getLogger()->error("Failed to parse transactions: {}", e.what());
        return result;
    }

    // Check if the root is a dictionary
    if (!root.isDictionary()) {
        getLogger()->error("Transactions file is not a dictionary");
        return result;
    }

    // Get the dictionary
    const auto& dict = root.getDictionary();

    // Add the transactions from the dictionary
    for (const auto& [transactionID, transactionValue] : dict) {
        // Check if the value is a dictionary
        if (!transactionValue->isDictionary()) {
            getLogger()->warning("Transaction is not a dictionary");
            continue;
        }

        // Get the dictionary
        const auto& transactionDict = transactionValue->getDictionary();

        // Deserialize the transaction
        auto serializedTransaction = deserializeTransaction(transactionDict);
        if (!serializedTransaction) {
            getLogger()->warning("Failed to deserialize transaction: {}", transactionID);
            continue;
        }

        // Add the transaction to the result
        result[transactionID] = serializedTransaction;
    }

    getLogger()->info("Loaded {} transactions from {}", result.size(), filePath);
    return result;
}

bool DHTTransactionManager::saveTransactions(const std::string& filePath) const {
    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);
    return saveTransactionsToFile(m_transactions, filePath);
}

bool DHTTransactionManager::loadTransactions(const std::string& filePath) {
    // Load the serialized transactions
    auto serializedTransactions = loadTransactionsFromFile(filePath);
    if (serializedTransactions.empty()) {
        return false;
    }

    // Clear the current transactions
    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);
    m_transactions.clear();

    // Convert the serialized transactions to actual transactions
    for (const auto& entry : serializedTransactions) {
        // Use the transaction ID as the key
        const auto& serializedTransaction = entry.second;

        // Create a query based on the transaction type
        std::shared_ptr<QueryMessage> query;

        switch (serializedTransaction->queryMethod) {
            case QueryMethod::Ping:
                query = std::make_shared<PingQuery>();
                break;
            case QueryMethod::FindNode: {
                // Convert the string to a NodeID
                NodeID targetID;
                if (serializedTransaction->targetID.size() != targetID.size()) {
                    getLogger()->warning("Invalid target ID size: {}", serializedTransaction->targetID.size());
                    continue;
                }

                std::copy(serializedTransaction->targetID.begin(), serializedTransaction->targetID.end(), targetID.begin());

                // Create the query
                query = std::make_shared<FindNodeQuery>(targetID);
                break;
            }
            case QueryMethod::GetPeers: {
                // Convert the string to an InfoHash
                InfoHash infoHash;
                if (serializedTransaction->infoHash.size() != infoHash.size()) {
                    getLogger()->warning("Invalid info hash size: {}", serializedTransaction->infoHash.size());
                    continue;
                }

                std::copy(serializedTransaction->infoHash.begin(), serializedTransaction->infoHash.end(), infoHash.begin());

                // Create the query
                query = std::make_shared<GetPeersQuery>(infoHash);
                break;
            }
            case QueryMethod::AnnouncePeer: {
                // Convert the string to an InfoHash
                InfoHash infoHash;
                if (serializedTransaction->infoHash.size() != infoHash.size()) {
                    getLogger()->warning("Invalid info hash size: {}", serializedTransaction->infoHash.size());
                    continue;
                }

                std::copy(serializedTransaction->infoHash.begin(), serializedTransaction->infoHash.end(), infoHash.begin());

                // Create the query
                auto announceQuery = std::make_shared<AnnouncePeerQuery>(
                    infoHash,
                    serializedTransaction->port
                );

                announceQuery->setToken(serializedTransaction->token);
                query = announceQuery;
                break;
            }
            default:
                getLogger()->warning("Unknown query type: {}", static_cast<int>(serializedTransaction->queryMethod));
                continue;
        }

        // Set the transaction ID
        query->setTransactionID(serializedTransaction->id);

        // Parse the endpoint
        auto colonPos = serializedTransaction->endpoint.find(':');
        if (colonPos == std::string::npos) {
            getLogger()->warning("Invalid endpoint: {}", serializedTransaction->endpoint);
            continue;
        }

        std::string ipStr = serializedTransaction->endpoint.substr(0, colonPos);
        std::string portStr = serializedTransaction->endpoint.substr(colonPos + 1);

        network::NetworkAddress address;
        if (!address.fromString(ipStr)) {
            getLogger()->warning("Invalid IP: {}", ipStr);
            continue;
        }

        uint16_t port;
        try {
            port = static_cast<uint16_t>(std::stoi(portStr));
        } catch (const std::exception& e) {
            getLogger()->warning("Invalid port: {}", portStr);
            continue;
        }

        network::EndPoint endpoint(address, port);

        // Create the transaction
        auto transaction = std::make_shared<Transaction>(
            serializedTransaction->id,
            query,
            endpoint,
            nullptr, // No response callback
            nullptr, // No error callback
            nullptr  // No timeout callback
        );

        // Add the transaction to the map
        m_transactions[serializedTransaction->id] = transaction;
    }

    getLogger()->info("Loaded {} transactions", m_transactions.size());
    return true;
}

} // namespace dht_hunter::dht
