#pragma once

#include "dht_hunter/unified_event/event_processor.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace dht_hunter::unified_event {

/**
 * @brief Configuration for the logging processor
 */
struct LoggingProcessorConfig {
    std::string logFilePath;
    EventSeverity minSeverity = EventSeverity::Trace; // Changed from Debug to Trace
    bool consoleOutput = true;
    bool fileOutput = false;
    bool includeTimestamp = true;
    bool includeSeverity = true;
    bool includeSource = true;
};

/**
 * @brief Processor for logging events
 */
class LoggingProcessor : public EventProcessor {
public:
    /**
     * @brief Constructor
     * @param config The logging configuration
     */
    explicit LoggingProcessor(const LoggingProcessorConfig& config = LoggingProcessorConfig());

    /**
     * @brief Destructor
     */
    ~LoggingProcessor() override;

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
     * @brief Sets the logging configuration
     * @param config The logging configuration
     */
    void setConfig(const LoggingProcessorConfig& config);

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
     * @brief Sets the minimum severity level for logging
     * @param severity The minimum severity level
     */
    void setMinSeverity(EventSeverity severity);

    /**
     * @brief Gets the minimum severity level for logging
     * @return The minimum severity level
     */
    EventSeverity getMinSeverity() const;

    /**
     * @brief Sets whether to output to the console
     * @param enable Whether to enable console output
     */
    void setConsoleOutput(bool enable);

    /**
     * @brief Sets whether to output to a file
     * @param enable Whether to enable file output
     * @param filePath The path to the log file (if empty, uses the configured path)
     */
    void setFileOutput(bool enable, const std::string& filePath = "");

private:
    /**
     * @brief Formats an event for logging
     * @param event The event to format
     * @return The formatted log message
     */
    std::string formatEvent(std::shared_ptr<Event> event) const;

    /**
     * @brief Converts a message type to a string
     * @param type The message type
     * @return The string representation of the message type
     */
    std::string messageTypeToString(dht_hunter::dht::MessageType type) const;

    /**
     * @brief Writes a log message to the console
     * @param message The message to write
     * @param severity The severity of the message
     */
    void writeToConsole(const std::string& message, EventSeverity severity) const;

    /**
     * @brief Writes a log message to the file
     * @param message The message to write
     */
    void writeToFile(const std::string& message);

    LoggingProcessorConfig m_config;
    std::ofstream m_logFile;
    mutable std::mutex m_logFileMutex;

    // ANSI color codes for console output
    static const std::unordered_map<EventSeverity, std::string> SEVERITY_COLORS;
};

} // namespace dht_hunter::unified_event
