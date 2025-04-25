#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <optional>

namespace dht_hunter::dht {

// Forward declarations
class ResponseMessage;
class ErrorMessage;
class QueryMessage;

/**
 * @brief Callback for DHT responses
 */
using ResponseCallback = std::function<void(std::shared_ptr<ResponseMessage>)>;

/**
 * @brief Callback for DHT errors
 */
using ErrorCallback = std::function<void(std::shared_ptr<ErrorMessage>)>;

/**
 * @brief Callback for DHT timeouts
 */
using TimeoutCallback = std::function<void()>;

/**
 * @brief Represents the state of a transaction
 */
enum class TransactionState {
    Pending,    ///< Transaction is pending a response
    Completed,  ///< Transaction completed successfully
    Failed,     ///< Transaction failed
    TimedOut    ///< Transaction timed out
};

/**
 * @brief Represents a transaction
 */
struct TransactionInfo {
    std::string id;                                  ///< Transaction ID
    std::shared_ptr<QueryMessage> query;             ///< Query message
    network::EndPoint endpoint;                      ///< Endpoint the query was sent to
    std::chrono::steady_clock::time_point creationTime; ///< Time the transaction was created
    TransactionState state;                          ///< Current state of the transaction
    std::optional<std::string> errorMessage;         ///< Error message if the transaction failed
    std::optional<int> errorCode;                    ///< Error code if the transaction failed
};

/**
 * @brief Represents a transaction with callbacks
 */
struct TransactionWithCallbacks : public TransactionInfo {
    ResponseCallback responseCallback;               ///< Callback for responses
    ErrorCallback errorCallback;                     ///< Callback for errors
    TimeoutCallback timeoutCallback;                 ///< Callback for timeouts
};

/**
 * @brief Represents a serialized transaction for persistence
 */
struct SerializedTransaction {
    std::string id;                                  ///< Transaction ID
    QueryMethod queryMethod;                         ///< Query method
    std::string targetID;                            ///< Target ID for find_node queries
    std::string infoHash;                            ///< Info hash for get_peers and announce_peer queries
    uint16_t port;                                   ///< Port for announce_peer queries
    std::string token;                               ///< Token for announce_peer queries
    std::string endpoint;                            ///< Endpoint the query was sent to
    int64_t creationTime;                            ///< Time the transaction was created (milliseconds since epoch)
    TransactionState state;                          ///< Current state of the transaction
    std::optional<std::string> errorMessage;         ///< Error message if the transaction failed
    std::optional<int> errorCode;                    ///< Error code if the transaction failed
};

} // namespace dht_hunter::dht
