#pragma once

#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/util/filesystem_utils.hpp"

namespace dht_hunter::logforge {

/**
 * @class LogInitializer
 * @brief Singleton class to ensure logger is initialized before any other action.
 *
 * This class uses the Meyers singleton pattern to ensure that the logger
 * is initialized during static initialization, before main() is called.
 */
class LogInitializer {
public:
    /**
     * @brief Get the singleton instance of LogInitializer.
     * @return Reference to the singleton instance.
     */
    static LogInitializer& getInstance() {
        static LogInitializer instance;
        return instance;
    }

    /**
     * @brief Initialize the logging system with standard settings.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param useColors Whether to use colored output in the console.
     * @param async Whether to use asynchronous logging.
     */
    void initializeLogger(
        const LogLevel consoleLevel = LogLevel::TRACE,
        const LogLevel fileLevel = LogLevel::TRACE,
        const std::string& filename = "",  // Empty string means use executable name
        bool useColors = true,
        const bool async = false
    ) {
        if (!m_initialized) {
            LogForge::init(consoleLevel, fileLevel, filename, useColors, async);
            m_initialized = true;
        }
    }

    /**
     * @brief Check if the logger has been initialized.
     * @return True if the logger has been initialized, false otherwise.
     */
    bool isInitialized() const {
        return m_initialized;
    }

private:
    /**
     * @brief Private constructor to enforce singleton pattern.
     */
    LogInitializer() : m_initialized(false) {}

    /**
     * @brief Private destructor.
     */
    ~LogInitializer() = default;

    /**
     * @brief Delete copy constructor.
     */
    LogInitializer(const LogInitializer&) = delete;

    /**
     * @brief Delete assignment operator.
     */
    LogInitializer& operator=(const LogInitializer&) = delete;

    /**
     * @brief Flag to track if the logger has been initialized.
     */
    bool m_initialized;
};

/**
 * @brief Helper function to get the LogInitializer instance.
 * @return Reference to the LogInitializer singleton.
 */
inline LogInitializer& getLogInitializer() {
    return LogInitializer::getInstance();
}

} // namespace dht_hunter::logforge
