#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <string>
#include <memory>
#include <functional>
#include <chrono>

namespace dht_hunter::dht::transactions {

/**
 * @brief Callback for transaction responses
 */
using TransactionResponseCallback = std::function<void(std::shared_ptr<ResponseMessage>, const EndPoint&)>;

/**
 * @brief Callback for transaction errors
 */
using TransactionErrorCallback = std::function<void(std::shared_ptr<ErrorMessage>, const EndPoint&)>;

/**
 * @brief Callback for transaction timeouts
 */
using TransactionTimeoutCallback = std::function<void()>;

/**
 * @brief A transaction
 */
class Transaction {
public:
    /**
     * @brief Constructs a transaction
     * @param id The transaction ID
     * @param query The query message
     * @param endpoint The endpoint
     * @param responseCallback The response callback
     * @param errorCallback The error callback
     * @param timeoutCallback The timeout callback
     */
    Transaction(const std::string& id,
               std::shared_ptr<QueryMessage> query,
               const EndPoint& endpoint,
               TransactionResponseCallback responseCallback,
               TransactionErrorCallback errorCallback,
               TransactionTimeoutCallback timeoutCallback);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getID() const;

    /**
     * @brief Gets the query message
     * @return The query message
     */
    std::shared_ptr<QueryMessage> getQuery() const;

    /**
     * @brief Gets the endpoint
     * @return The endpoint
     */
    const EndPoint& getEndpoint() const;

    /**
     * @brief Gets the timestamp
     * @return The timestamp
     */
    const std::chrono::steady_clock::time_point& getTimestamp() const;

    /**
     * @brief Gets the response callback
     * @return The response callback
     */
    const TransactionResponseCallback& getResponseCallback() const;

    /**
     * @brief Gets the error callback
     * @return The error callback
     */
    const TransactionErrorCallback& getErrorCallback() const;

    /**
     * @brief Gets the timeout callback
     * @return The timeout callback
     */
    const TransactionTimeoutCallback& getTimeoutCallback() const;

    /**
     * @brief Checks if the transaction has timed out
     * @param timeout The timeout in seconds
     * @return True if the transaction has timed out, false otherwise
     */
    bool hasTimedOut(int timeout) const;

private:
    std::string m_id;
    std::shared_ptr<QueryMessage> m_query;
    EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_timestamp;
    TransactionResponseCallback m_responseCallback;
    TransactionErrorCallback m_errorCallback;
    TransactionTimeoutCallback m_timeoutCallback;
};

} // namespace dht_hunter::dht::transactions
