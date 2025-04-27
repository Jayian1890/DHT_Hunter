#include "dht_hunter/unified_event/processors/statistics_processor.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <iomanip>

namespace dht_hunter::unified_event {

StatisticsProcessor::StatisticsProcessor(const StatisticsProcessorConfig& config)
    : m_config(config),
      m_totalEventCount(0),
      m_totalProcessingTime(0),
      m_processedEventCount(0),
      m_running(false) {
}

StatisticsProcessor::~StatisticsProcessor() {
    shutdown();
}

std::string StatisticsProcessor::getId() const {
    return "statistics";
}

bool StatisticsProcessor::shouldProcess(std::shared_ptr<Event> event) const {
    // Process all events for statistics
    return event != nullptr;
}

void StatisticsProcessor::process(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    // Record the start time for processing time tracking
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Increment the total event count
    m_totalEventCount++;
    
    // Track event counts by type if configured
    if (m_config.trackEventCounts) {
        EventType eventType = event->getType();
        
        // Use operator[] to create the atomic if it doesn't exist
        // This is thread-safe because the map itself is protected by the mutex
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        m_eventCounts[eventType]++;
    }
    
    // Track event counts by severity if configured
    if (m_config.trackSeverityCounts) {
        EventSeverity severity = event->getSeverity();
        
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        m_severityCounts[severity]++;
    }
    
    // Track event counts by source if configured
    if (m_config.trackSourceCounts) {
        const std::string& source = event->getSource();
        
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        m_sourceCounts[source]++;
    }
    
    // Track processing time if configured
    if (m_config.trackProcessingTimes) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        m_totalProcessingTime += duration.count();
        m_processedEventCount++;
    }
}

bool StatisticsProcessor::initialize() {
    // Start the logging thread if configured
    if (m_config.logInterval > 0) {
        m_running = true;
        m_loggingThread = std::thread(&StatisticsProcessor::logStatisticsPeriodically, this);
    }
    
    return true;
}

void StatisticsProcessor::shutdown() {
    // Stop the logging thread if running
    if (m_running) {
        m_running = false;
        
        if (m_loggingThread.joinable()) {
            m_loggingThread.join();
        }
    }
}

size_t StatisticsProcessor::getTotalEventCount() const {
    return m_totalEventCount;
}

size_t StatisticsProcessor::getEventCount(EventType eventType) const {
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    
    auto it = m_eventCounts.find(eventType);
    if (it != m_eventCounts.end()) {
        return it->second;
    }
    
    return 0;
}

size_t StatisticsProcessor::getSeverityCount(EventSeverity severity) const {
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    
    auto it = m_severityCounts.find(severity);
    if (it != m_severityCounts.end()) {
        return it->second;
    }
    
    return 0;
}

size_t StatisticsProcessor::getSourceCount(const std::string& source) const {
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    
    auto it = m_sourceCounts.find(source);
    if (it != m_sourceCounts.end()) {
        return it->second;
    }
    
    return 0;
}

double StatisticsProcessor::getAverageProcessingTime() const {
    size_t processedCount = m_processedEventCount;
    if (processedCount == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_totalProcessingTime) / processedCount;
}

std::string StatisticsProcessor::getStatisticsAsJson() const {
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"totalEventCount\": " << m_totalEventCount << ",\n";
    
    // Add event counts by type
    if (m_config.trackEventCounts) {
        ss << "  \"eventCounts\": {\n";
        
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        bool first = true;
        for (const auto& pair : m_eventCounts) {
            if (!first) {
                ss << ",\n";
            }
            ss << "    \"" << eventTypeToString(pair.first) << "\": " << pair.second;
            first = false;
        }
        
        ss << "\n  },\n";
    }
    
    // Add event counts by severity
    if (m_config.trackSeverityCounts) {
        ss << "  \"severityCounts\": {\n";
        
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        bool first = true;
        for (const auto& pair : m_severityCounts) {
            if (!first) {
                ss << ",\n";
            }
            ss << "    \"" << eventSeverityToString(pair.first) << "\": " << pair.second;
            first = false;
        }
        
        ss << "\n  },\n";
    }
    
    // Add event counts by source
    if (m_config.trackSourceCounts) {
        ss << "  \"sourceCounts\": {\n";
        
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        bool first = true;
        for (const auto& pair : m_sourceCounts) {
            if (!first) {
                ss << ",\n";
            }
            ss << "    \"" << pair.first << "\": " << pair.second;
            first = false;
        }
        
        ss << "\n  },\n";
    }
    
    // Add processing time statistics
    if (m_config.trackProcessingTimes) {
        ss << "  \"averageProcessingTime\": " << std::fixed << std::setprecision(2) << getAverageProcessingTime() << "\n";
    } else {
        ss << "  \"averageProcessingTime\": 0.0\n";
    }
    
    ss << "}";
    
    return ss.str();
}

void StatisticsProcessor::resetStatistics() {
    m_totalEventCount = 0;
    m_totalProcessingTime = 0;
    m_processedEventCount = 0;
    
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    m_eventCounts.clear();
    m_severityCounts.clear();
    m_sourceCounts.clear();
}

void StatisticsProcessor::logStatisticsPeriodically() {
    while (m_running) {
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::seconds(m_config.logInterval));
        
        if (!m_running) {
            break;
        }
        
        // Log the statistics
        std::cout << "Event Statistics:\n" << getStatisticsAsJson() << std::endl;
    }
}

} // namespace dht_hunter::unified_event
