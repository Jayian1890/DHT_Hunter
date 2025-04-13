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


namespace dht_hunter::logging {

/**
 * @enum LogLevel
 * @brief Defines the severity levels for logging.
 */
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    OFF
};

/**
 * @brief Converts a LogLevel to its string representation.
 * @param level The log level to convert.
 * @return String representation of the log level.
 */
inline std::string logLevelToString(const LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return "TRACE";
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::OFF:      return "OFF";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief Converts a string to its corresponding LogLevel.
 * @param levelStr The string to convert.
 * @return The corresponding LogLevel, or LogLevel::INFO if not recognized.
 */
inline LogLevel stringToLogLevel(const std::string& levelStr) {
    if (levelStr == "TRACE")     return LogLevel::TRACE;
    if (levelStr == "DEBUG")     return LogLevel::DEBUG;
    if (levelStr == "INFO")      return LogLevel::INFO;
    if (levelStr == "WARNING")   return LogLevel::WARNING;
    if (levelStr == "ERROR")     return LogLevel::ERROR;
    if (levelStr == "CRITICAL")  return LogLevel::CRITICAL;
    if (levelStr == "OFF")       return LogLevel::OFF;
    return LogLevel::INFO; // Default
}

/**
 * @class LogSink
 * @brief Abstract base class for log sinks.
 *
 * A log sink is a destination for log messages, such as console, file, etc.
 */
class LogSink {
public:
    virtual ~LogSink() = default;

    /**
     * @brief Writes a log message to the sink.
     * @param level The severity level of the message.
     * @param loggerName The name of the logger.
     * @param message The log message.
     * @param timestamp The timestamp of the log message.
     */
    virtual void write(LogLevel level, const std::string& loggerName,
                      const std::string& message,
                      const std::chrono::system_clock::time_point& timestamp) = 0;

    /**
     * @brief Sets the minimum log level for this sink.
     * @param level The minimum log level.
     */
    void setLevel(const LogLevel level) {
        m_level = level;
    }

    /**
     * @brief Gets the minimum log level for this sink.
     * @return The minimum log level.
     */
    [[nodiscard]] LogLevel getLevel() const {
        return m_level;
    }

    /**
     * @brief Checks if a message with the given level should be logged.
     * @param level The level to check.
     * @return True if the message should be logged, false otherwise.
     */
    [[nodiscard]] bool shouldLog(const LogLevel level) const {
        return level >= m_level;
    }

protected:
    LogLevel m_level = LogLevel::INFO;
};

/**
 * @class ConsoleSink
 * @brief A log sink that writes to the console with color support.
 */
class ConsoleSink : public LogSink {
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
class FileSink : public LogSink {
public:
    /**
     * @brief Constructs a FileSink.
     * @param filename The name of the file to write to.
     * @param append Whether to append to the file or overwrite it.
     */
    explicit FileSink(const std::string& filename, bool append = true) {
        auto mode = append ? std::ios::app : std::ios::trunc;
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

        const std::lock_guard<std::mutex> lock(m_mutex);

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
 * @class Logger
 * @brief Main logger class that handles log messages and distributes them to sinks.
 */
class Logger {
public:
    /**
     * @brief Virtual destructor for proper cleanup.
     */
    virtual ~Logger() = default;
    /**
     * @brief Initializes the logging system.
     * @param consoleLevel The minimum log level for the console sink.
     * @param fileLevel The minimum log level for the file sink.
     * @param filename The name of the log file.
     * @param useColors Whether to use colored output in the console.
     */
    static void init(LogLevel consoleLevel = LogLevel::INFO,
                    LogLevel fileLevel = LogLevel::DEBUG,
                    const std::string& filename = "dht_hunter.log",
                    bool useColors = true) {
        const std::lock_guard<std::mutex> lock(s_mutex);

        // Create default sinks if not already initialized
        if (s_initialized) {
            return;
        }

        // Create console sink with color support
        auto consoleSink = std::make_shared<ConsoleSink>(useColors);
        consoleSink->setLevel(consoleLevel);
        addSink(consoleSink);

        // Create file sink
        auto fileSink = std::make_shared<FileSink>(filename);
        fileSink->setLevel(fileLevel);
        addSink(fileSink);

        s_initialized = true;
    }

    /**
     * @brief Adds a sink to the logging system.
     * @param sink The sink to add.
     */
    static void addSink(const std::shared_ptr<LogSink> &sink) {
        const std::lock_guard<std::mutex> lock(s_mutex);
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
     * @brief Gets a logger with the specified name.
     * @param name The name of the logger.
     * @return A shared pointer to the logger.
     */
    static std::shared_ptr<Logger> getLogger(const std::string& name) {
        const std::lock_guard<std::mutex> lock(s_mutex);

        // Initialize if not already done
        if (!s_initialized) {
            init();
        }

        // Check if logger already exists
        auto it = s_loggers.find(name);
        if (it != s_loggers.end()) {
            return it->second;
        }

        // Create new logger
        auto logger = std::shared_ptr<Logger>(new Logger(name));
        s_loggers[name] = logger;
        return logger;
    }

    /**
     * @brief Sets the global log level for all loggers.
     * @param level The log level to set.
     */
    static void setGlobalLevel(LogLevel level) {
        const std::lock_guard<std::mutex> lock(s_mutex);
        s_globalLevel = level;
    }

    /**
     * @brief Sets the log level for this logger.
     * @param level The log level to set.
     */
    void setLevel(LogLevel level) {
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

private:
    /**
     * @brief Constructs a Logger with the specified name.
     * @param name The name of the logger.
     */
    explicit Logger(std::string name) : m_name(std::move(name)) {}

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

        // Write to all sinks
        for (const auto& sink : s_sinks) {
            sink->write(level, m_name, message, timestamp);
        }
    }

    /**
     * @brief Simple string formatting function.
     * @param format The format string.
     * @param args The arguments to format.
     * @return The formatted string.
     */
    template<typename... Args>
    static std::string formatString(const std::string& format, [[maybe_unused]] Args&&... args) {
        // For simplicity, we'll just return the format string for now
        // In a real implementation, you would replace {} placeholders with args
        return format;
    }

    std::string m_name;
    LogLevel m_level = LogLevel::TRACE;

    static std::mutex s_mutex;
    static std::vector<std::shared_ptr<LogSink>> s_sinks;
    static std::unordered_map<std::string, std::shared_ptr<Logger>> s_loggers;
    static LogLevel s_globalLevel;
    static bool s_initialized;
};

} // namespace dht_hunter::logging

