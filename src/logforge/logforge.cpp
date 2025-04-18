#include "dht_hunter/logforge/logforge.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <thread>

namespace dht_hunter::logforge {

// Initialize static members
std::mutex LogForge::s_mutex;
std::vector<std::shared_ptr<LogSink>> LogForge::s_sinks;
std::unordered_map<std::string, std::shared_ptr<LogForge>> LogForge::s_loggers;
LogLevel LogForge::s_globalLevel = LogLevel::TRACE;
bool LogForge::s_initialized = false;
bool LogForge::s_asyncLoggingEnabled;

// Async logging implementation
namespace {
    // Message structure for the async queue
    struct AsyncLogMessage {
        LogLevel level;
        std::string loggerName;
        std::string message;
        std::chrono::system_clock::time_point timestamp;

        AsyncLogMessage(LogLevel lvl, std::string name, std::string msg,
                       std::chrono::system_clock::time_point ts)
            : level(lvl), loggerName(std::move(name)), message(std::move(msg)),
              timestamp(ts) {}
    };

    // Async logging state
    std::mutex g_queueMutex;
    std::condition_variable g_queueCV;
    std::deque<AsyncLogMessage> g_messageQueue;
    std::thread g_workerThread;
    std::atomic<bool> g_running{true};
    std::atomic<bool> g_flushRequested{false};
    std::mutex g_flushMutex;
    std::condition_variable g_flushCV;
    std::atomic<size_t> g_maxQueueSize{10000};
    std::atomic<size_t> g_messagesDropped{0};

    // Worker thread function
    void processQueue() {
        while (g_running) {
            std::deque<AsyncLogMessage> messagesToProcess;

            {
                std::unique_lock<std::mutex> lock(g_queueMutex);

                // Wait for messages or stop signal
                g_queueCV.wait(lock, [] {
                    return !g_messageQueue.empty() || !g_running || g_flushRequested;
                });

                if (!g_running && g_messageQueue.empty()) {
                    break;
                }

                // Swap queues to minimize lock time
                messagesToProcess.swap(g_messageQueue);
            }

            // Process all messages
            for (const auto& msg : messagesToProcess) {
                for (const auto& sink : LogForge::getSinks()) {
                    if (sink->shouldLog(msg.level)) {
                        sink->write(msg.level, msg.loggerName, msg.message, msg.timestamp);
                    }
                }
            }

            // Handle flush request
            if (g_flushRequested) {
                std::lock_guard<std::mutex> lock(g_flushMutex);
                g_flushRequested = false;
                g_flushCV.notify_one();
            }
        }
    }

    // Flag to track if worker thread is initialized
    std::atomic<bool> g_workerThreadInitialized{false};

    // Initialize the worker thread
    void initWorkerThread() {
        if (!g_workerThreadInitialized.exchange(true)) {
            g_workerThread = std::thread(processQueue);
        }
    }

    // Cleanup function to be called at program exit
    class AsyncLoggerCleanup {
    public:
        ~AsyncLoggerCleanup() {
            g_running = false;
            g_queueCV.notify_one();

            if (g_workerThread.joinable()) {
                g_workerThread.join();
            }
        }
    };

    // Static instance to ensure cleanup at program exit
    // This variable is intentionally not referenced elsewhere in the code
    // It's used for its destructor, which will be called when the program exits
    [[maybe_unused]] AsyncLoggerCleanup cleanup;
}

// Implementation of the flush method
void LogForge::flush() {
    if (s_asyncLoggingEnabled) {
        std::unique_lock<std::mutex> lock(g_flushMutex);
        g_flushRequested = true;
        g_queueCV.notify_one();
        g_flushCV.wait(lock, [] { return !g_flushRequested; });
    }
}

// Implementation of the isAsyncLoggingEnabled method
bool LogForge::isAsyncLoggingEnabled() {
    return s_asyncLoggingEnabled;
}

// Implementation of the shutdown method
void LogForge::shutdown() {
    // First, flush any pending messages
    if (s_asyncLoggingEnabled) {
        flush();
    }

    // Stop the worker thread
    g_running = false;
    g_queueCV.notify_one();

    // Wait for the worker thread to finish
    if (g_workerThread.joinable()) {
        g_workerThread.join();
    }

    // Clear all sinks and loggers
    const std::lock_guard lock(s_mutex);
    s_sinks.clear();
    s_loggers.clear();
    s_initialized = false;

    // Reset the worker thread initialization flag
    g_workerThreadInitialized = false;

    // Reset async state
    g_messageQueue.clear();
    g_flushRequested = false;
    g_running = true;
    g_messagesDropped = 0;
}

// Implementation of the queueAsyncMessage method
void LogForge::queueAsyncMessage(LogLevel level, const std::string& loggerName,
                               const std::string& message,
                               const std::chrono::system_clock::time_point& timestamp) {
    // Initialize the worker thread if needed
    initWorkerThread();

    // Queue the message
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_messageQueue.emplace_back(level, loggerName, message, timestamp);

        // If queue gets too large, remove oldest messages
        if (g_messageQueue.size() > g_maxQueueSize) {
            g_messageQueue.pop_front();
            ++g_messagesDropped;
        }
    }

    // Notify the worker thread
    g_queueCV.notify_one();
}

} // namespace dht_hunter::logforge
