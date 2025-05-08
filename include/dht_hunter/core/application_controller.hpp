#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "dht_hunter/bittorrent/metadata/metadata_acquisition_manager.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/core/persistence_manager.hpp"
#include "dht_hunter/network/udp_server.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/utility/thread/thread_pool.hpp"
#include "dht_hunter/web/web_server.hpp"

namespace dht_hunter::core {

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

/**
 * @brief Task information structure
 */
struct TaskInfo {
    std::string id;
    std::string name;
    TaskPriority priority;
    std::chrono::steady_clock::time_point scheduledTime;
    std::chrono::milliseconds interval;
    bool recurring;
    std::function<void()> task;
    std::atomic<bool> cancelled;
};

/**
 * @brief Application controller class
 *
 * Manages the application lifecycle and provides centralized control
 * for all application components. Implements async task execution for
 * improved performance.
 */
class ApplicationController {
public:
    /**
     * @brief Get the singleton instance
     * @return Shared pointer to the ApplicationController instance
     */
    static std::shared_ptr<ApplicationController> getInstance();

    /**
     * @brief Destructor
     */
    ~ApplicationController();

    /**
     * @brief Initialize the application
     * @param configFile Path to the configuration file
     * @param webRoot Path to the web root directory
     * @param webPort Optional web server port (overrides config file)
     * @param logLevel Optional log level (overrides config file)
     * @return True if initialization was successful, false otherwise
     */
    bool initialize(const std::string& configFile, const std::string& webRoot, uint16_t webPort = 0,
                   const std::string& logLevel = "");

    /**
     * @brief Generate default configuration file
     * @return True if the configuration was generated successfully, false otherwise
     */
    bool generateDefaultConfig();

    /**
     * @brief Start the application
     * @return True if start was successful, false otherwise
     */
    bool start();

    /**
     * @brief Stop the application
     * @return True if stop was successful, false otherwise
     */
    bool stop();

    /**
     * @brief Pause the application (suspend non-critical operations)
     * @return True if pause was successful, false otherwise
     */
    bool pause();

    /**
     * @brief Resume the application after pause
     * @return True if resume was successful, false otherwise
     */
    bool resume();

    /**
     * @brief Check if the application is running
     * @return True if the application is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Check if the application is paused
     * @return True if the application is paused, false otherwise
     */
    bool isPaused() const;

    /**
     * @brief Get the DHT node
     * @return Shared pointer to the DHT node
     */
    std::shared_ptr<dht::DHTNode> getDHTNode() const;

    /**
     * @brief Get the web server
     * @return Shared pointer to the web server
     */
    std::shared_ptr<web::WebServer> getWebServer() const;

    /**
     * @brief Get the metadata acquisition manager
     * @return Shared pointer to the metadata acquisition manager
     */
    std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> getMetadataManager() const;

    /**
     * @brief Schedule a one-time task
     * @param name Task name for identification
     * @param task Function to execute
     * @param delayMs Delay in milliseconds before execution
     * @param priority Task priority
     * @return Task ID for cancellation
     */
    std::string scheduleTask(
        const std::string& name,
        std::function<void()> task,
        uint64_t delayMs = 0,
        TaskPriority priority = TaskPriority::NORMAL);

    /**
     * @brief Schedule a recurring task
     * @param name Task name for identification
     * @param task Function to execute
     * @param intervalMs Interval in milliseconds between executions
     * @param delayMs Initial delay in milliseconds before first execution
     * @param priority Task priority
     * @return Task ID for cancellation
     */
    std::string scheduleRecurringTask(
        const std::string& name,
        std::function<void()> task,
        uint64_t intervalMs,
        uint64_t delayMs = 0,
        TaskPriority priority = TaskPriority::NORMAL);

    /**
     * @brief Cancel a scheduled task
     * @param taskId ID of the task to cancel
     * @return True if the task was found and cancelled, false otherwise
     */
    bool cancelTask(const std::string& taskId);

    /**
     * @brief Execute a task asynchronously
     * @param task Function to execute
     * @return Future for the task result
     */
    template<typename Func>
    auto executeAsync(Func&& task) -> std::future<decltype(task())> {
        return m_threadPool->enqueue(std::forward<Func>(task));
    }

    /**
     * @brief Handle system sleep notification
     */
    void handleSystemSleep();

    /**
     * @brief Handle system wake notification
     */
    void handleSystemWake();

    /**
     * @brief Update application title with statistics
     */
    void updateTitle();

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    ApplicationController();

    /**
     * @brief Initialize the thread pool
     * @param threadCount Number of threads to use
     */
    void initializeThreadPool(size_t threadCount);

    /**
     * @brief Task scheduler thread function
     */
    void taskSchedulerThread();

    /**
     * @brief Generate a unique task ID
     * @return Unique task ID
     */
    std::string generateTaskId() const;

    /**
     * @brief Load configuration settings
     */
    void loadConfiguration();

    /**
     * @brief Register event handlers
     */
    void registerEventHandlers();

    /**
     * @brief Configure logging settings from configuration
     */
    void configureLogging();

    // Singleton instance
    static std::shared_ptr<ApplicationController> s_instance;
    static std::mutex s_instanceMutex;

    // Application state
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_systemSleeping;

    // Configuration
    std::string m_configFile;
    std::string m_webRoot;
    std::string m_overrideLogLevel;
    uint16_t m_webPort;
    uint16_t m_dhtPort;

    // Components
    std::shared_ptr<utility::config::ConfigurationManager> m_configManager;
    std::shared_ptr<network::UDPServer> m_udpServer;
    std::shared_ptr<dht::DHTNode> m_dhtNode;
    std::shared_ptr<dht::PersistenceManager> m_persistenceManager;
    std::shared_ptr<web::WebServer> m_webServer;
    std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> m_metadataManager;

    // Thread pool for async task execution
    std::shared_ptr<utility::thread::ThreadPool> m_threadPool;

    // Task scheduling
    std::mutex m_tasksMutex;
    std::condition_variable m_tasksCondition;
    std::vector<std::shared_ptr<TaskInfo>> m_tasks;
    std::atomic<bool> m_schedulerRunning;
    std::thread m_schedulerThread;

    // Statistics
    std::chrono::steady_clock::time_point m_lastTitleUpdateTime;
    std::chrono::steady_clock::time_point m_lastActivityCheckTime;
};

} // namespace dht_hunter::core
