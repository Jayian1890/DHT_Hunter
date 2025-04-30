#pragma once

#include "dht_hunter/unified_event/event_processor.hpp"
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>

namespace dht_hunter::unified_event {

/**
 * @brief Configuration for the statistics processor
 */
struct StatisticsProcessorConfig {
    // Whether to track event counts by type
    bool trackEventCounts = true;

    // Whether to track event counts by severity
    bool trackSeverityCounts = true;

    // Whether to track event counts by source
    bool trackSourceCounts = true;

    // Whether to track event processing times
    bool trackProcessingTimes = true;

    // How often to log statistics (in seconds, 0 = never)
    unsigned int logInterval = 0;
};

/**
 * @brief Processor for collecting event statistics
 */
class StatisticsProcessor : public EventProcessor {
public:
    /**
     * @brief Constructor
     * @param config The statistics processor configuration
     */
    explicit StatisticsProcessor(const StatisticsProcessorConfig& config = StatisticsProcessorConfig());

    /**
     * @brief Destructor
     */
    ~StatisticsProcessor() override;

    /**
     * @brief Gets the processor ID
     * @return The processor ID
     */
    std::string getId() const override;

    /**
     * @brief Determines if the processor should handle the event
     * @param event The event to check
     * @return True if the processor should handle the event, false otherwise
     */
    bool shouldProcess(std::shared_ptr<Event> event) const override;

    /**
     * @brief Processes the event
     * @param event The event to process
     */
    void process(std::shared_ptr<Event> event) override;

    /**
     * @brief Initializes the processor
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the processor
     */
    void shutdown() override;

    /**
     * @brief Gets the total number of events processed
     * @return The total number of events processed
     */
    size_t getTotalEventCount() const;

    /**
     * @brief Gets the number of events processed by type
     * @param eventType The event type
     * @return The number of events processed of the specified type
     */
    size_t getEventCount(EventType eventType) const;

    /**
     * @brief Gets the number of events processed by severity
     * @param severity The event severity
     * @return The number of events processed of the specified severity
     */
    size_t getSeverityCount(EventSeverity severity) const;

    /**
     * @brief Gets the number of events processed by source
     * @param source The event source
     * @return The number of events processed from the specified source
     */
    size_t getSourceCount(const std::string& source) const;

    /**
     * @brief Gets the average processing time for events
     * @return The average processing time in microseconds
     */
    double getAverageProcessingTime() const;

    /**
     * @brief Gets all statistics as a formatted string
     * @return The statistics as a formatted string
     */
    std::string getStatisticsAsString() const;

    /**
     * @brief Resets all statistics
     */
    void resetStatistics();

private:
    /**
     * @brief Logs statistics periodically
     */
    void logStatisticsPeriodically();

    StatisticsProcessorConfig m_config;

    // Statistics
    std::atomic<size_t> m_totalEventCount;
    std::unordered_map<EventType, std::atomic<size_t>> m_eventCounts;
    std::unordered_map<EventSeverity, std::atomic<size_t>> m_severityCounts;
    std::unordered_map<std::string, std::atomic<size_t>> m_sourceCounts;

    // Processing time statistics
    std::atomic<size_t> m_totalProcessingTime;
    std::atomic<size_t> m_processedEventCount;

    // Mutex for thread safety
    mutable std::mutex m_statisticsMutex;

    // Logging thread
    std::atomic<bool> m_running;
    std::thread m_loggingThread;
};

} // namespace dht_hunter::unified_event
