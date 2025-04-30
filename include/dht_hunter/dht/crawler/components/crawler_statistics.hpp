#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>

namespace dht_hunter::dht::crawler {

/**
 * @brief Statistics for the crawler
 */
class CrawlerStatistics {
public:
    /**
     * @brief Constructs crawler statistics with default values
     */
    CrawlerStatistics();

    /**
     * @brief Gets the number of nodes discovered
     * @return The number of nodes discovered
     */
    size_t getNodesDiscovered() const;

    /**
     * @brief Sets the number of nodes discovered
     * @param nodesDiscovered The number of nodes discovered
     */
    void setNodesDiscovered(size_t nodesDiscovered);

    /**
     * @brief Gets the number of nodes that responded
     * @return The number of nodes that responded
     */
    size_t getNodesResponded() const;

    /**
     * @brief Sets the number of nodes that responded
     * @param nodesResponded The number of nodes that responded
     */
    void setNodesResponded(size_t nodesResponded);

    /**
     * @brief Gets the number of info hashes discovered
     * @return The number of info hashes discovered
     */
    size_t getInfoHashesDiscovered() const;

    /**
     * @brief Sets the number of info hashes discovered
     * @param infoHashesDiscovered The number of info hashes discovered
     */
    void setInfoHashesDiscovered(size_t infoHashesDiscovered);

    /**
     * @brief Gets the number of peers discovered
     * @return The number of peers discovered
     */
    size_t getPeersDiscovered() const;

    /**
     * @brief Sets the number of peers discovered
     * @param peersDiscovered The number of peers discovered
     */
    void setPeersDiscovered(size_t peersDiscovered);

    /**
     * @brief Gets the number of queries sent
     * @return The number of queries sent
     */
    size_t getQueriesSent() const;

    /**
     * @brief Sets the number of queries sent
     * @param queriesSent The number of queries sent
     */
    void setQueriesSent(size_t queriesSent);

    /**
     * @brief Gets the number of responses received
     * @return The number of responses received
     */
    size_t getResponsesReceived() const;

    /**
     * @brief Sets the number of responses received
     * @param responsesReceived The number of responses received
     */
    void setResponsesReceived(size_t responsesReceived);

    /**
     * @brief Gets the number of errors received
     * @return The number of errors received
     */
    size_t getErrorsReceived() const;

    /**
     * @brief Sets the number of errors received
     * @param errorsReceived The number of errors received
     */
    void setErrorsReceived(size_t errorsReceived);

    /**
     * @brief Gets the number of timeouts
     * @return The number of timeouts
     */
    size_t getTimeouts() const;

    /**
     * @brief Sets the number of timeouts
     * @param timeouts The number of timeouts
     */
    void setTimeouts(size_t timeouts);

    /**
     * @brief Gets the start time
     * @return The start time
     */
    std::chrono::steady_clock::time_point getStartTime() const;

    /**
     * @brief Sets the start time
     * @param startTime The start time
     */
    void setStartTime(const std::chrono::steady_clock::time_point& startTime);

    /**
     * @brief Increments the number of nodes discovered
     * @param count The number to increment by (default: 1)
     */
    void incrementNodesDiscovered(size_t count = 1);

    /**
     * @brief Increments the number of nodes that responded
     * @param count The number to increment by (default: 1)
     */
    void incrementNodesResponded(size_t count = 1);

    /**
     * @brief Increments the number of info hashes discovered
     * @param count The number to increment by (default: 1)
     */
    void incrementInfoHashesDiscovered(size_t count = 1);

    /**
     * @brief Increments the number of peers discovered
     * @param count The number to increment by (default: 1)
     */
    void incrementPeersDiscovered(size_t count = 1);

    /**
     * @brief Increments the number of queries sent
     * @param count The number to increment by (default: 1)
     */
    void incrementQueriesSent(size_t count = 1);

    /**
     * @brief Increments the number of responses received
     * @param count The number to increment by (default: 1)
     */
    void incrementResponsesReceived(size_t count = 1);

    /**
     * @brief Increments the number of errors received
     * @param count The number to increment by (default: 1)
     */
    void incrementErrorsReceived(size_t count = 1);

    /**
     * @brief Increments the number of timeouts
     * @param count The number to increment by (default: 1)
     */
    void incrementTimeouts(size_t count = 1);

    /**
     * @brief Resets the statistics
     */
    void reset();

private:
    // Number of nodes discovered
    size_t m_nodesDiscovered;
    
    // Number of nodes that responded
    size_t m_nodesResponded;
    
    // Number of info hashes discovered
    size_t m_infoHashesDiscovered;
    
    // Number of peers discovered
    size_t m_peersDiscovered;
    
    // Number of queries sent
    size_t m_queriesSent;
    
    // Number of responses received
    size_t m_responsesReceived;
    
    // Number of errors received
    size_t m_errorsReceived;
    
    // Number of timeouts
    size_t m_timeouts;
    
    // Start time
    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace dht_hunter::dht::crawler
