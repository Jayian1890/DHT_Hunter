#include "dht_hunter/unified_event/processors/logging_processor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace dht_hunter::unified_event {

// ANSI color codes for console output
const std::unordered_map<EventSeverity, std::string> LoggingProcessor::SEVERITY_COLORS = {
    {EventSeverity::Debug, "\033[37m"},     // White
    {EventSeverity::Info, "\033[32m"},      // Green
    {EventSeverity::Warning, "\033[33m"},   // Yellow
    {EventSeverity::Error, "\033[31m"},     // Red
    {EventSeverity::Critical, "\033[35m"}   // Magenta
};

LoggingProcessor::LoggingProcessor(const LoggingProcessorConfig& config)
    : m_config(config) {
}

LoggingProcessor::~LoggingProcessor() {
    shutdown();
}

std::string LoggingProcessor::getId() const {
    return "logging";
}

bool LoggingProcessor::shouldProcess(std::shared_ptr<Event> event) const {
    if (!event) {
        return false;
    }
    
    // Check if the event severity is at or above the minimum severity
    return event->getSeverity() >= m_config.minSeverity;
}

void LoggingProcessor::process(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    std::string message = formatEvent(event);
    
    if (m_config.consoleOutput) {
        writeToConsole(message, event->getSeverity());
    }
    
    if (m_config.fileOutput) {
        writeToFile(message);
    }
}

bool LoggingProcessor::initialize() {
    if (m_config.fileOutput && !m_config.logFilePath.empty()) {
        std::lock_guard<std::mutex> lock(m_logFileMutex);
        
        m_logFile.open(m_config.logFilePath, std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << m_config.logFilePath << std::endl;
            return false;
        }
    }
    
    return true;
}

void LoggingProcessor::shutdown() {
    if (m_logFile.is_open()) {
        std::lock_guard<std::mutex> lock(m_logFileMutex);
        m_logFile.close();
    }
}

void LoggingProcessor::setMinSeverity(EventSeverity severity) {
    m_config.minSeverity = severity;
}

EventSeverity LoggingProcessor::getMinSeverity() const {
    return m_config.minSeverity;
}

void LoggingProcessor::setConsoleOutput(bool enable) {
    m_config.consoleOutput = enable;
}

void LoggingProcessor::setFileOutput(bool enable, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_logFileMutex);
    
    // Close the current log file if open
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    
    m_config.fileOutput = enable;
    
    if (!filePath.empty()) {
        m_config.logFilePath = filePath;
    }
    
    if (enable && !m_config.logFilePath.empty()) {
        m_logFile.open(m_config.logFilePath, std::ios::app);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << m_config.logFilePath << std::endl;
            m_config.fileOutput = false;
        }
    }
}

std::string LoggingProcessor::formatEvent(std::shared_ptr<Event> event) const {
    std::stringstream ss;
    
    // Add timestamp if configured
    if (m_config.includeTimestamp) {
        auto timeT = std::chrono::system_clock::to_time_t(event->getTimestamp());
        auto timeInfo = std::localtime(&timeT);
        ss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S") << " ";
    }
    
    // Add severity if configured
    if (m_config.includeSeverity) {
        ss << "[" << eventSeverityToString(event->getSeverity()) << "] ";
    }
    
    // Add source if configured
    if (m_config.includeSource) {
        ss << "[" << event->getSource() << "] ";
    }
    
    // Add event name and message
    ss << event->getName();
    
    // Add custom message if available
    auto message = event->getProperty<std::string>("message");
    if (message && !message->empty()) {
        ss << ": " << *message;
    }
    
    return ss.str();
}

void LoggingProcessor::writeToConsole(const std::string& message, EventSeverity severity) const {
    // Get the color for this severity
    auto colorIt = SEVERITY_COLORS.find(severity);
    std::string colorCode = colorIt != SEVERITY_COLORS.end() ? colorIt->second : "";
    
    // Reset color code
    const std::string resetCode = "\033[0m";
    
    // Write to the appropriate stream based on severity
    if (severity >= EventSeverity::Error) {
        std::cerr << colorCode << message << resetCode << std::endl;
    } else {
        std::cout << colorCode << message << resetCode << std::endl;
    }
}

void LoggingProcessor::writeToFile(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logFileMutex);
    
    if (m_logFile.is_open()) {
        m_logFile << message << std::endl;
        m_logFile.flush();
    }
}

} // namespace dht_hunter::unified_event
