#include "dht_hunter/dht/crawler/components/crawler_statistics.hpp"

namespace dht_hunter::dht::crawler {

CrawlerStatistics::CrawlerStatistics()
    : m_nodesDiscovered(0),
      m_nodesResponded(0),
      m_infoHashesDiscovered(0),
      m_peersDiscovered(0),
      m_queriesSent(0),
      m_responsesReceived(0),
      m_errorsReceived(0),
      m_timeouts(0),
      m_startTime(std::chrono::steady_clock::now()) {
}

size_t CrawlerStatistics::getNodesDiscovered() const {
    return m_nodesDiscovered;
}

void CrawlerStatistics::setNodesDiscovered(size_t nodesDiscovered) {
    m_nodesDiscovered = nodesDiscovered;
}

size_t CrawlerStatistics::getNodesResponded() const {
    return m_nodesResponded;
}

void CrawlerStatistics::setNodesResponded(size_t nodesResponded) {
    m_nodesResponded = nodesResponded;
}

size_t CrawlerStatistics::getInfoHashesDiscovered() const {
    return m_infoHashesDiscovered;
}

void CrawlerStatistics::setInfoHashesDiscovered(size_t infoHashesDiscovered) {
    m_infoHashesDiscovered = infoHashesDiscovered;
}

size_t CrawlerStatistics::getPeersDiscovered() const {
    return m_peersDiscovered;
}

void CrawlerStatistics::setPeersDiscovered(size_t peersDiscovered) {
    m_peersDiscovered = peersDiscovered;
}

size_t CrawlerStatistics::getQueriesSent() const {
    return m_queriesSent;
}

void CrawlerStatistics::setQueriesSent(size_t queriesSent) {
    m_queriesSent = queriesSent;
}

size_t CrawlerStatistics::getResponsesReceived() const {
    return m_responsesReceived;
}

void CrawlerStatistics::setResponsesReceived(size_t responsesReceived) {
    m_responsesReceived = responsesReceived;
}

size_t CrawlerStatistics::getErrorsReceived() const {
    return m_errorsReceived;
}

void CrawlerStatistics::setErrorsReceived(size_t errorsReceived) {
    m_errorsReceived = errorsReceived;
}

size_t CrawlerStatistics::getTimeouts() const {
    return m_timeouts;
}

void CrawlerStatistics::setTimeouts(size_t timeouts) {
    m_timeouts = timeouts;
}

std::chrono::steady_clock::time_point CrawlerStatistics::getStartTime() const {
    return m_startTime;
}

void CrawlerStatistics::setStartTime(const std::chrono::steady_clock::time_point& startTime) {
    m_startTime = startTime;
}

void CrawlerStatistics::incrementNodesDiscovered(size_t count) {
    m_nodesDiscovered += count;
}

void CrawlerStatistics::incrementNodesResponded(size_t count) {
    m_nodesResponded += count;
}

void CrawlerStatistics::incrementInfoHashesDiscovered(size_t count) {
    m_infoHashesDiscovered += count;
}

void CrawlerStatistics::incrementPeersDiscovered(size_t count) {
    m_peersDiscovered += count;
}

void CrawlerStatistics::incrementQueriesSent(size_t count) {
    m_queriesSent += count;
}

void CrawlerStatistics::incrementResponsesReceived(size_t count) {
    m_responsesReceived += count;
}

void CrawlerStatistics::incrementErrorsReceived(size_t count) {
    m_errorsReceived += count;
}

void CrawlerStatistics::incrementTimeouts(size_t count) {
    m_timeouts += count;
}

void CrawlerStatistics::reset() {
    m_nodesDiscovered = 0;
    m_nodesResponded = 0;
    m_infoHashesDiscovered = 0;
    m_peersDiscovered = 0;
    m_queriesSent = 0;
    m_responsesReceived = 0;
    m_errorsReceived = 0;
    m_timeouts = 0;
    m_startTime = std::chrono::steady_clock::now();
}

} // namespace dht_hunter::dht::crawler
