#include "dht_hunter/dht/transactions/components/transaction.hpp"

namespace dht_hunter::dht::transactions {

Transaction::Transaction(const std::string& id,
                       std::shared_ptr<QueryMessage> query,
                       const EndPoint& endpoint,
                       TransactionResponseCallback responseCallback,
                       TransactionErrorCallback errorCallback,
                       TransactionTimeoutCallback timeoutCallback)
    : m_id(id),
      m_query(query),
      m_endpoint(endpoint),
      m_timestamp(std::chrono::steady_clock::now()),
      m_responseCallback(responseCallback),
      m_errorCallback(errorCallback),
      m_timeoutCallback(timeoutCallback) {
}

const std::string& Transaction::getID() const {
    return m_id;
}

std::shared_ptr<QueryMessage> Transaction::getQuery() const {
    return m_query;
}

const EndPoint& Transaction::getEndpoint() const {
    return m_endpoint;
}

const std::chrono::steady_clock::time_point& Transaction::getTimestamp() const {
    return m_timestamp;
}

const TransactionResponseCallback& Transaction::getResponseCallback() const {
    return m_responseCallback;
}

const TransactionErrorCallback& Transaction::getErrorCallback() const {
    return m_errorCallback;
}

const TransactionTimeoutCallback& Transaction::getTimeoutCallback() const {
    return m_timeoutCallback;
}

bool Transaction::hasTimedOut(int timeout) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_timestamp).count();
    return elapsed >= timeout;
}

} // namespace dht_hunter::dht::transactions
