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

std::string LoggingProcessor::messageTypeToString(dht_hunter::dht::MessageType type) const {
    switch (type) {
        case dht_hunter::dht::MessageType::Query:
            return "Query";
        case dht_hunter::dht::MessageType::Response:
            return "Response";
        case dht_hunter::dht::MessageType::Error:
            return "Error";
        default:
            return "Unknown";
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

    // Add event name
    ss << event->getName();

    // Add detailed information based on event type
    switch (event->getType()) {
        case EventType::MessageReceived: {
            auto message = event->getProperty<std::shared_ptr<dht_hunter::dht::Message>>("message");
            auto sender = event->getProperty<dht_hunter::network::NetworkAddress>("sender");

            if (message && sender) {
                ss << " - Type: " << messageTypeToString((*message)->getType());

                // Add query method for responses
                if ((*message)->getType() == dht_hunter::dht::MessageType::Response) {
                    auto response = std::dynamic_pointer_cast<dht_hunter::dht::ResponseMessage>(*message);
                    if (response) {
                        // Try to determine what kind of response this is
                        if (std::dynamic_pointer_cast<dht_hunter::dht::PingResponse>(response)) {
                            ss << " ping";
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::FindNodeResponse>(response)) {
                            ss << " find_node";
                            auto findNodeResponse = std::dynamic_pointer_cast<dht_hunter::dht::FindNodeResponse>(response);
                            if (findNodeResponse) {
                                ss << ", nodes: " << findNodeResponse->getNodes().size();
                            }
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::GetPeersResponse>(response)) {
                            ss << " get_peers";
                            auto getPeersResponse = std::dynamic_pointer_cast<dht_hunter::dht::GetPeersResponse>(response);
                            if (getPeersResponse) {
                                if (getPeersResponse->hasNodes()) {
                                    ss << ", nodes: " << getPeersResponse->getNodes().size();
                                }
                                if (getPeersResponse->hasPeers()) {
                                    ss << ", peers: " << getPeersResponse->getPeers().size();
                                }
                            }
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::AnnouncePeerResponse>(response)) {
                            ss << " announce_peer";
                        }
                    }
                }
                // Add query method for queries
                else if ((*message)->getType() == dht_hunter::dht::MessageType::Query) {
                    auto query = std::dynamic_pointer_cast<dht_hunter::dht::QueryMessage>(*message);
                    if (query) {
                        switch (query->getMethod()) {
                            case dht_hunter::dht::QueryMethod::Ping:
                                ss << " ping";
                                break;
                            case dht_hunter::dht::QueryMethod::FindNode: {
                                ss << " find_node";
                                auto findNodeQuery = std::dynamic_pointer_cast<dht_hunter::dht::FindNodeQuery>(query);
                                if (findNodeQuery) {
                                    std::string targetIDStr = dht_hunter::dht::nodeIDToString(findNodeQuery->getTargetID());
                                    std::string shortTargetID = targetIDStr.substr(0, 6);
                                    ss << ", target: " << shortTargetID;
                                }
                                break;
                            }
                            case dht_hunter::dht::QueryMethod::GetPeers: {
                                ss << " get_peers";
                                auto getPeersQuery = std::dynamic_pointer_cast<dht_hunter::dht::GetPeersQuery>(query);
                                if (getPeersQuery) {
                                    std::string infoHashStr = dht_hunter::dht::infoHashToString(getPeersQuery->getInfoHash());
                                    std::string shortInfoHash = infoHashStr.substr(0, 6);
                                    ss << ", info_hash: " << shortInfoHash;
                                }
                                break;
                            }
                            case dht_hunter::dht::QueryMethod::AnnouncePeer: {
                                ss << " announce_peer";
                                auto announceQuery = std::dynamic_pointer_cast<dht_hunter::dht::AnnouncePeerQuery>(query);
                                if (announceQuery) {
                                    std::string infoHashStr = dht_hunter::dht::infoHashToString(announceQuery->getInfoHash());
                                    std::string shortInfoHash = infoHashStr.substr(0, 6);
                                    ss << ", info_hash: " << shortInfoHash;
                                    ss << ", port: " << announceQuery->getPort();
                                }
                                break;
                            }
                        }
                    }
                }

                ss << ", from: " << (*sender).toString();

                // Add node ID if available
                if ((*message)->getNodeID()) {
                    std::string nodeIDStr = dht_hunter::dht::nodeIDToString((*message)->getNodeID().value());
                    std::string shortNodeID = nodeIDStr.substr(0, 6);
                    ss << ", nodeID: " << shortNodeID;
                }

                // Add transaction ID
                std::string transactionID = (*message)->getTransactionID();
                if (!transactionID.empty()) {
                    ss << ", txID: " << transactionID;
                }
            }
            break;
        }
        case EventType::MessageSent: {
            auto message = event->getProperty<std::shared_ptr<dht_hunter::dht::Message>>("message");
            auto recipient = event->getProperty<dht_hunter::network::NetworkAddress>("recipient");

            if (message && recipient) {
                ss << " - Type: " << messageTypeToString((*message)->getType());

                // Add query method for responses
                if ((*message)->getType() == dht_hunter::dht::MessageType::Response) {
                    auto response = std::dynamic_pointer_cast<dht_hunter::dht::ResponseMessage>(*message);
                    if (response) {
                        // Try to determine what kind of response this is
                        if (std::dynamic_pointer_cast<dht_hunter::dht::PingResponse>(response)) {
                            ss << " ping";
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::FindNodeResponse>(response)) {
                            ss << " find_node";
                            auto findNodeResponse = std::dynamic_pointer_cast<dht_hunter::dht::FindNodeResponse>(response);
                            if (findNodeResponse) {
                                ss << ", nodes: " << findNodeResponse->getNodes().size();
                            }
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::GetPeersResponse>(response)) {
                            ss << " get_peers";
                            auto getPeersResponse = std::dynamic_pointer_cast<dht_hunter::dht::GetPeersResponse>(response);
                            if (getPeersResponse) {
                                if (getPeersResponse->hasNodes()) {
                                    ss << ", nodes: " << getPeersResponse->getNodes().size();
                                }
                                if (getPeersResponse->hasPeers()) {
                                    ss << ", peers: " << getPeersResponse->getPeers().size();
                                }
                            }
                        } else if (std::dynamic_pointer_cast<dht_hunter::dht::AnnouncePeerResponse>(response)) {
                            ss << " announce_peer";
                        }
                    }
                }
                // Add query method for queries
                else if ((*message)->getType() == dht_hunter::dht::MessageType::Query) {
                    auto query = std::dynamic_pointer_cast<dht_hunter::dht::QueryMessage>(*message);
                    if (query) {
                        switch (query->getMethod()) {
                            case dht_hunter::dht::QueryMethod::Ping:
                                ss << " ping";
                                break;
                            case dht_hunter::dht::QueryMethod::FindNode: {
                                ss << " find_node";
                                auto findNodeQuery = std::dynamic_pointer_cast<dht_hunter::dht::FindNodeQuery>(query);
                                if (findNodeQuery) {
                                    std::string targetIDStr = dht_hunter::dht::nodeIDToString(findNodeQuery->getTargetID());
                                    std::string shortTargetID = targetIDStr.substr(0, 6);
                                    ss << ", target: " << shortTargetID;
                                }
                                break;
                            }
                            case dht_hunter::dht::QueryMethod::GetPeers: {
                                ss << " get_peers";
                                auto getPeersQuery = std::dynamic_pointer_cast<dht_hunter::dht::GetPeersQuery>(query);
                                if (getPeersQuery) {
                                    std::string infoHashStr = dht_hunter::dht::infoHashToString(getPeersQuery->getInfoHash());
                                    std::string shortInfoHash = infoHashStr.substr(0, 6);
                                    ss << ", info_hash: " << shortInfoHash;
                                }
                                break;
                            }
                            case dht_hunter::dht::QueryMethod::AnnouncePeer: {
                                ss << " announce_peer";
                                auto announceQuery = std::dynamic_pointer_cast<dht_hunter::dht::AnnouncePeerQuery>(query);
                                if (announceQuery) {
                                    std::string infoHashStr = dht_hunter::dht::infoHashToString(announceQuery->getInfoHash());
                                    std::string shortInfoHash = infoHashStr.substr(0, 6);
                                    ss << ", info_hash: " << shortInfoHash;
                                    ss << ", port: " << announceQuery->getPort();
                                }
                                break;
                            }
                        }
                    }
                }

                ss << ", to: " << (*recipient).toString();

                // Add node ID if available
                if ((*message)->getNodeID()) {
                    std::string nodeIDStr = dht_hunter::dht::nodeIDToString((*message)->getNodeID().value());
                    std::string shortNodeID = nodeIDStr.substr(0, 6);
                    ss << ", nodeID: " << shortNodeID;
                }

                // Add transaction ID
                std::string transactionID = (*message)->getTransactionID();
                if (!transactionID.empty()) {
                    ss << ", txID: " << transactionID;
                }
            }
            break;
        }
        default: {
            // Use the details method for other event types
            std::string details = event->getDetails();
            if (!details.empty()) {
                ss << " - " << details;
            }
            break;
        }
    }

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
