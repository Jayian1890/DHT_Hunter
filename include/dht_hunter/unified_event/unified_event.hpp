#pragma once

// Core event system
#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/unified_event/event_processor.hpp"
#include "dht_hunter/unified_event/components/event_bus.hpp"

// Event processors
#include "dht_hunter/unified_event/processors/logging_processor.hpp"
#include "dht_hunter/unified_event/processors/component_processor.hpp"
#include "dht_hunter/unified_event/processors/statistics_processor.hpp"

// Event types
#include "dht_hunter/unified_event/events/system_events.hpp"
#include "dht_hunter/unified_event/events/log_events.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include "dht_hunter/unified_event/events/message_events.hpp"
#include "dht_hunter/unified_event/events/peer_events.hpp"

namespace dht_hunter::unified_event {

/**
 * @brief Initializes the unified event system
 * @param enableLogging Whether to enable the logging processor
 * @param enableComponent Whether to enable the component processor
 * @param enableStatistics Whether to enable the statistics processor
 * @param asyncProcessing Whether to process events asynchronously
 * @return True if initialization was successful, false otherwise
 */
inline bool initializeEventSystem(
    bool enableLogging = true,
    bool enableComponent = true,
    bool enableStatistics = true,
    bool asyncProcessing = false
) {
    // Create the event bus
    EventBusConfig eventBusConfig;
    eventBusConfig.asyncProcessing = asyncProcessing;
    auto eventBus = EventBus::getInstance(eventBusConfig);

    // Start the event bus
    if (!eventBus->start()) {
        return false;
    }

    // Add processors
    if (enableLogging) {
        LoggingProcessorConfig loggingConfig;
        loggingConfig.consoleOutput = true;
        loggingConfig.fileOutput = false;
        loggingConfig.minSeverity = EventSeverity::Debug;

        auto loggingProcessor = std::make_shared<LoggingProcessor>(loggingConfig);
        if (!eventBus->addProcessor(loggingProcessor)) {
            return false;
        }
    }

    if (enableComponent) {
        ComponentProcessorConfig componentConfig;
        auto componentProcessor = std::make_shared<ComponentProcessor>(componentConfig);
        if (!eventBus->addProcessor(componentProcessor)) {
            return false;
        }
    }

    if (enableStatistics) {
        StatisticsProcessorConfig statisticsConfig;
        statisticsConfig.logInterval = 0; // Don't log automatically
        auto statisticsProcessor = std::make_shared<StatisticsProcessor>(statisticsConfig);
        if (!eventBus->addProcessor(statisticsProcessor)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Shuts down the unified event system
 */
inline void shutdownEventSystem() {
    auto eventBus = EventBus::getInstance();
    eventBus->stop();
}

/**
 * @brief Gets the statistics processor
 * @return The statistics processor, or nullptr if not found
 */
inline std::shared_ptr<StatisticsProcessor> getStatisticsProcessor() {
    auto eventBus = EventBus::getInstance();
    return std::dynamic_pointer_cast<StatisticsProcessor>(eventBus->getProcessor("statistics"));
}

/**
 * @brief Gets the logging processor
 * @return The logging processor, or nullptr if not found
 */
inline std::shared_ptr<LoggingProcessor> getLoggingProcessor() {
    auto eventBus = EventBus::getInstance();
    return std::dynamic_pointer_cast<LoggingProcessor>(eventBus->getProcessor("logging"));
}

/**
 * @brief Configures the logging processor with the specified configuration
 * @param config The logging processor configuration
 * @return True if the configuration was successful, false otherwise
 */
inline bool configureLoggingProcessor(const LoggingProcessorConfig& config) {
    auto loggingProcessor = getLoggingProcessor();
    if (loggingProcessor) {
        loggingProcessor->setConfig(config);
        return true;
    }
    return false;
}

/**
 * @brief Gets the component processor
 * @return The component processor, or nullptr if not found
 */
inline std::shared_ptr<ComponentProcessor> getComponentProcessor() {
    auto eventBus = EventBus::getInstance();
    return std::dynamic_pointer_cast<ComponentProcessor>(eventBus->getProcessor("component"));
}

/**
 * @brief Logs a message through the unified event system
 * @param source The source of the message
 * @param severity The severity of the message
 * @param message The message to log
 */
inline void logMessage(const std::string& source, EventSeverity severity, const std::string& message) {
    auto eventBus = EventBus::getInstance();
    auto event = std::make_shared<LogMessageEvent>(source, severity, message);
    eventBus->publish(event);
}

/**
 * @brief Logs a trace message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logTrace(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Trace, message);
}

/**
 * @brief Logs a debug message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logDebug(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Debug, message);
}

/**
 * @brief Logs an info message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logInfo(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Info, message);
}

/**
 * @brief Logs a warning message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logWarning(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Warning, message);
}

/**
 * @brief Logs an error message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logError(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Error, message);
}

/**
 * @brief Logs a critical message through the unified event system
 * @param source The source of the message
 * @param message The message to log
 */
inline void logCritical(const std::string& source, const std::string& message) {
    logMessage(source, EventSeverity::Critical, message);
}

} // namespace dht_hunter::unified_event
