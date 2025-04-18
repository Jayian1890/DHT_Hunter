#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "formatter.hpp"
#include "log_common.hpp"
#include "rotating_file_sink.hpp"

namespace dht_hunter::logforge {

/**
 * @class ConsoleSink
 * @brief A log sink that writes to the console with color support.
 */
class ConsoleSink final : public LogSink {
public:
    explicit ConsoleSink(const bool useColors = true) : m_useColors(useColors) {}

    void write(const LogLevel level, const std::string& loggerName,
              const std::string& message,
              const std::chrono::system_clock::time_point& timestamp) override {
        if (!shouldLog(level)) {
            return;
        }

        const std::lock_guard lock(m_mutex);

        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
            << std::setfill('0') << std::setw(3) << ms.count() << " ";

        // Add colored level if colors are enabled
        if (m_useColors) {
            oss << getColorCode(level) << "[" << logLevelToString(level) << "]" << getColorReset() << " ";
        } else {
            oss << "[" << logLevelToString(level) << "] ";
        }

        oss << "[" << loggerName << "] "
            << message;

        // Output to console
        if (level >= LogLevel::ERROR) {
            std::cerr << oss.str() << std::endl;
        } else {
            std::cout << oss.str() << std::endl;
        }
    }

    /**
     * @brief Enables or disables colored output.
     * @param useColors Whether to use colors.
     */
    void setUseColors(const bool useColors) {
        m_useColors = useColors;
    }

    /**
     * @brief Checks if colored output is enabled.
     * @return True if colored output is enabled, false otherwise.
     */
    [[nodiscard]] bool getUseColors() const {
        return m_useColors;
    }

private:
    /**
     * @brief Gets the ANSI color code for a log level.
     * @param level The log level.
     * @return The ANSI color code.
     */
    [[nodiscard]] static std::string getColorCode(const LogLevel level) {
        switch (level) {
            case LogLevel::TRACE:    return "\033[90m";  // Dark gray
            case LogLevel::DEBUG:    return "\033[36m";  // Cyan
            case LogLevel::INFO:     return "\033[32m";  // Green
            case LogLevel::WARNING:  return "\033[33m";  // Yellow
            case LogLevel::ERROR:    return "\033[31m";  // Red
            case LogLevel::CRITICAL: return "\033[1;31m"; // Bright red
            default:                 return "\033[0m";   // Reset
        }
    }

    /**
     * @brief Gets the ANSI reset code.
     * @return The ANSI reset code.
     */
    [[nodiscard]] static std::string getColorReset() {
        return "\033[0m";
    }

    std::mutex m_mutex;
    bool m_useColors;
};

/**
 * @class FileSink
 * @brief A log sink that writes to a file.
 */
class FileSink final : public LogSink {
public:
    /**
     * @brief Constructs a FileSink.
     * @param filename The name of the file to write to.
     * @param append Whether to append to the file or overwrite it.
     */
    explicit FileSink(const std::string& filename, const bool append = true) {
        const auto mode = append ? std::ios::app : std::ios::trunc;
        m_file.open(filename, std::ios::out | mode);
        if (!m_file.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }

    ~FileSink() override {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void write(const LogLevel level, const std::string& loggerName,
              const std::string& message,
              const std::chrono::system_clock::time_point& timestamp) override {
        if (!shouldLog(level) || !m_file.is_open()) {
            return;
        }

        const std::lock_guard lock(m_mutex);

        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        m_file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
               << std::setfill('0') << std::setw(3) << ms.count() << " "
               << "[" << logLevelToString(level) << "] "
               << "[" << loggerName << "] "
               << message << std::endl;

        // Flush to ensure the message is written immediately
        m_file.flush();
    }

private:
    std::ofstream m_file;
    std::mutex m_mutex;
};

/**
 * @class LogForge
 * @brief Main logger class that handles log messages and distributes them to sinks.
 */
class LogForge final {
public:
    /**
     * @brief Destructor for proper cleanup.
     */
    ~LogForge() = default;

    /**
     * @brief Initializes the logging system with a standard file sink.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param useColors Whether to use colored output in the console.
     * @param async Whether to use asynchronous logging.
     */
    static void init(const LogLevel consoleLevel = LogLevel::INFO,
                    const LogLevel fileLevel = LogLevel::DEBUG,
                    const std::string& filename = "dht_hunter.log",
                    bool useColors = true,
                    const bool async = true) {
        const std::lock_guard lock(s_mutex);

        // Create default sinks if not already initialized
        if (s_initialized) {
            return;
        }

        // Create console sink with color support
        const auto consoleSink = std::make_shared<ConsoleSink>(useColors);
        consoleSink->setLevel(consoleLevel);
        addSink(consoleSink);

        // Create file sink
        const auto fileSink = std::make_shared<FileSink>(filename);
        fileSink->setLevel(fileLevel);
        addSink(fileSink);

        // Set async mode
        s_asyncLoggingEnabled = async;
        std::cout << "LogForge::init called with async=" << (async ? "true" : "false") << std::endl;
        std::cout << "s_asyncLoggingEnabled set to " << (s_asyncLoggingEnabled ? "true" : "false") << std::endl;

        s_initialized = true;
    }

    /**
     * @brief Initializes the logging system with a size-based rotating file sink.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param maxSizeBytes The maximum size of a log file before rotation (in bytes).
     * @param maxFiles The maximum number of log files to keep.
     * @param useColors Whether to use colored output in the console.
     * @param async Whether to use asynchronous logging.
     */
    static void initWithSizeRotation(const LogLevel consoleLevel = LogLevel::INFO,
                                    const LogLevel fileLevel = LogLevel::DEBUG,
                                    const std::string& filename = "dht_hunter.log",
                                    size_t maxSizeBytes = 10 * 1024 * 1024,  // 10 MB default
                                    size_t maxFiles = 5,
                                    bool useColors = true,
                                    const bool async = true) {
        const std::lock_guard lock(s_mutex);

        // Create default sinks if not already initialized
        if (s_initialized) {
            return;
        }

        // Create console sink with color support
        const auto consoleSink = std::make_shared<ConsoleSink>(useColors);
        consoleSink->setLevel(consoleLevel);
        addSink(consoleSink);

        // Create rotating file sink with size-based rotation
        const auto fileSink = std::make_shared<RotatingFileSink>(filename, maxSizeBytes, maxFiles);
        fileSink->setLevel(fileLevel);
        addSink(fileSink);

        // Set async mode
        s_asyncLoggingEnabled = async;

        s_initialized = true;
    }

    /**
     * @brief Initializes the logging system with a time-based rotating file sink.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param rotationHours The time interval for rotation (in hours).
     * @param maxFiles The maximum number of log files to keep.
     * @param useColors Whether to use colored output in the console.
     * @param async Whether to use asynchronous logging.
     */
    static void initWithTimeRotation(const LogLevel consoleLevel = LogLevel::INFO,
                                    const LogLevel fileLevel = LogLevel::DEBUG,
                                    const std::string& filename = "dht_hunter.log",
                                    int rotationHours = 24,  // 24 hours default
                                    size_t maxFiles = 5,
                                    bool useColors = true,
                                    const bool async = true) {
        const std::lock_guard lock(s_mutex);

        // Create default sinks if not already initialized
        if (s_initialized) {
            return;
        }

        // Create console sink with color support
        const auto consoleSink = std::make_shared<ConsoleSink>(useColors);
        consoleSink->setLevel(consoleLevel);
        addSink(consoleSink);

        // Create rotating file sink with time-based rotation
        const auto fileSink = std::make_shared<RotatingFileSink>(filename, std::chrono::hours(rotationHours), maxFiles);
        fileSink->setLevel(fileLevel);
        addSink(fileSink);

        // Set async mode
        s_asyncLoggingEnabled = async;

        s_initialized = true;
    }

    /**
     * @brief Initializes the logging system with a rotating file sink that uses both size and time-based rotation.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param maxSizeBytes The maximum size of a log file before rotation (in bytes).
     * @param rotationHours The time interval for rotation (in hours).
     * @param maxFiles The maximum number of log files to keep.
     * @param useColors Whether to use colored output in the console.
     * @param async Whether to use asynchronous logging.
     */
    static void initWithSizeAndTimeRotation(const LogLevel consoleLevel = LogLevel::INFO,
                                           const LogLevel fileLevel = LogLevel::DEBUG,
                                           const std::string& filename = "dht_hunter.log",
                                           size_t maxSizeBytes = 10 * 1024 * 1024,  // 10 MB default
                                           int rotationHours = 24,  // 24 hours default
                                           size_t maxFiles = 5,
                                           bool useColors = true,
                                           const bool async = true) {
        const std::lock_guard lock(s_mutex);

        // Create default sinks if not already initialized
        if (s_initialized) {
            return;
        }

        // Create console sink with color support
        const auto consoleSink = std::make_shared<ConsoleSink>(useColors);
        consoleSink->setLevel(consoleLevel);
        addSink(consoleSink);

        // Create rotating file sink with both size and time-based rotation
        const auto fileSink = std::make_shared<RotatingFileSink>(filename, maxSizeBytes, std::chrono::hours(rotationHours), maxFiles);
        fileSink->setLevel(fileLevel);
        addSink(fileSink);

        // Set async mode
        s_asyncLoggingEnabled = async;

        s_initialized = true;
    }

    /**
     * @brief Gets a logger with the specified name.
     * @param name The name of the logger.
     * @return A shared pointer to the logger.
     */
    static std::shared_ptr<LogForge> getLogger(const std::string& name) {
        const std::lock_guard lock(s_mutex);

        // Initialize if not already done
        if (!s_initialized) {
            init();
        }

        // Check if logger already exists
        if (const auto it = s_loggers.find(name); it != s_loggers.end()) {
            return it->second;
        }

        // Create new logger
        auto logger = std::shared_ptr<LogForge>(new LogForge(name));
        s_loggers[name] = logger;
        return logger;
    }

    /**
     * @brief Adds a sink to the logging system.
     * @param sink The sink to add.
     */
    template<typename T>
    static void addSink(const std::shared_ptr<T> &sink) {
        static_assert(std::is_base_of_v<LogSink, T>, "Sink must inherit from LogSink");
        const std::lock_guard lock(s_mutex);
        s_sinks.push_back(sink);
    }

    /**
     * @brief Gets all registered sinks.
     * @return A reference to the vector of sinks.
     */
    static std::vector<std::shared_ptr<LogSink>>& getSinks() {
        return s_sinks;
    }

    /**
     * @brief Sets the global log level for all loggers.
     * @param level The log level to set.
     */
    static void setGlobalLevel(const LogLevel level) {
        const std::lock_guard lock(s_mutex);
        s_globalLevel = level;
    }

    /**
     * @brief Sets the log level for this logger.
     * @param level The log level to set.
     */
    void setLevel(const LogLevel level) {
        m_level = level;
    }

    /**
     * @brief Gets the log level for this logger.
     * @return The log level.
     */
    [[nodiscard]] LogLevel getLevel() const {
        return m_level;
    }

    /**
     * @brief Logs a message at the TRACE level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        log(LogLevel::TRACE, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a message at the DEBUG level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        log(LogLevel::DEBUG, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a message at the INFO level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        log(LogLevel::INFO, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a message at the WARNING level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void warning(const std::string& format, Args&&... args) {
        log(LogLevel::WARNING, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a message at the ERROR level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        log(LogLevel::ERROR, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Logs a message at the CRITICAL level.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void critical(const std::string& format, Args&&... args) {
        log(LogLevel::CRITICAL, format, std::forward<Args>(args)...);
    }

    /**
     * @brief Flushes all pending log messages.
     *
     * This method ensures that all queued log messages are written to their destinations.
     * It only has an effect when asynchronous logging is enabled.
     */
    static void flush();

    /**
     * @brief Checks if asynchronous logging is enabled.
     * @return True if asynchronous logging is enabled, false otherwise.
     */
    static bool isAsyncLoggingEnabled();

    /**
     * @brief Shuts down the logging system.
     *
     * This method stops the async logging thread if it's running,
     * flushes any pending messages, and clears all sinks and loggers.
     * It's particularly useful for testing and when you need to reset the logger state.
     */
    static void shutdown();

private:
    /**
     * @brief Constructs a LogForge with the specified name.
     * @param name The name of the logger.
     */
    explicit LogForge(std::string name) : m_name(std::move(name)) {}

    /**
     * @brief Logs a message at the specified level.
     * @param level The level to log at.
     * @param format The format string.
     * @param args The arguments to format.
     */
    template<typename... Args>
    void log(const LogLevel level, const std::string& format, Args&&... args) {
        // Check if this message should be logged
        if (level < m_level || level < s_globalLevel) {
            return;
        }

        // Format the message
        const std::string message = formatString(format, std::forward<Args>(args)...);

        // Get current timestamp
        const auto timestamp = std::chrono::system_clock::now();

        // Check if async logging is enabled
        if (s_asyncLoggingEnabled) {
            // Queue the message for asynchronous processing
            queueAsyncMessage(level, m_name, message, timestamp);
        } else {
            // Write to all sinks synchronously
            for (const auto& sink : s_sinks) {
                if (sink->shouldLog(level)) {
                    sink->write(level, m_name, message, timestamp);
                }
            }
        }
    }

    /**
     * @brief String formatting function that supports {} placeholders.
     * @param format The format string with {} placeholders.
     * @param args The arguments to format.
     * @return The formatted string.
     */
    template<typename... Args>
    static std::string formatString(const std::string& format, Args&&... args) {
        return Formatter::format(format, std::forward<Args>(args)...);
    }

    /**
     * @brief Queues a log message for asynchronous processing.
     * @param level The severity level of the message.
     * @param loggerName The name of the logger.
     * @param message The log message.
     * @param timestamp The timestamp of the log message.
     */
    static void queueAsyncMessage(LogLevel level, const std::string& loggerName,
                                 const std::string& message,
                                 const std::chrono::system_clock::time_point& timestamp);

    std::string m_name;
    LogLevel m_level = LogLevel::TRACE;

    static std::mutex s_mutex;
    static std::vector<std::shared_ptr<LogSink>> s_sinks;
    static std::unordered_map<std::string, std::shared_ptr<LogForge>> s_loggers;
    static LogLevel s_globalLevel;
    static bool s_initialized;
    static bool s_asyncLoggingEnabled;
};

} // namespace dht_hunter::logforge
