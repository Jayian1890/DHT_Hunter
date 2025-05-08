#include "dht_hunter/core/application_controller.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#include "dht_hunter/dht/services/statistics_service.hpp"
#include "dht_hunter/unified_event/events/custom_events.hpp"
#include "dht_hunter/unified_event/events/peer_events.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

namespace dht_hunter::core {

// Initialize static members
std::shared_ptr<ApplicationController> ApplicationController::s_instance = nullptr;
std::mutex ApplicationController::s_instanceMutex;

std::shared_ptr<ApplicationController> ApplicationController::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<ApplicationController>(new ApplicationController());
    }

    return s_instance;
}

ApplicationController::ApplicationController()
    : m_initialized(false),
      m_running(false),
      m_paused(false),
      m_systemSleeping(false),
      m_webPort(8080),
      m_dhtPort(6881),
      m_schedulerRunning(false) {

    // Initialize the thread pool with a default size
    initializeThreadPool(std::thread::hardware_concurrency());

    // Initialize timestamps
    m_lastTitleUpdateTime = std::chrono::steady_clock::now();
    m_lastActivityCheckTime = std::chrono::steady_clock::now();
}

ApplicationController::~ApplicationController() {
    // Ensure the application is stopped
    stop();
}

bool ApplicationController::initialize(const std::string& configFile, const std::string& webRoot) {
    if (m_initialized) {
        unified_event::logWarning("Core.ApplicationController", "Application already initialized");
        return true;
    }

    m_configFile = configFile;
    m_webRoot = webRoot;

    try {
        // Initialize the configuration manager
        m_configManager = utility::config::ConfigurationManager::getInstance(configFile);
        if (!m_configManager) {
            unified_event::logError("Core.ApplicationController", "Failed to initialize configuration manager");
            return false;
        }

        // Enable hot-reloading of the configuration file
        if (!m_configManager->enableHotReloading(true, 1000)) { // Check every second
            unified_event::logError("Core.ApplicationController", "Failed to enable hot-reloading of configuration file");
        } else {
            unified_event::logDebug("Core.ApplicationController", "Hot-reloading of configuration file enabled");
        }

        // Load configuration settings
        loadConfiguration();

        // Register event handlers
        registerEventHandlers();

        m_initialized = true;
        unified_event::logInfo("Core.ApplicationController", "Application initialized successfully");
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool ApplicationController::start() {
    if (!m_initialized) {
        unified_event::logError("Core.ApplicationController", "Cannot start uninitialized application");
        return false;
    }

    if (m_running) {
        unified_event::logWarning("Core.ApplicationController", "Application already running");
        return true;
    }

    try {
        unified_event::logInfo("Core.ApplicationController", "Starting application");

        // Create and start the UDP server
        m_udpServer = std::make_shared<network::UDPServer>(m_dhtPort);
        if (!m_udpServer->start()) {
            unified_event::logError("Core.ApplicationController", "Failed to start UDP server");
            return false;
        }

        // Create DHT configuration
        dht::DHTConfig dhtConfig(m_dhtPort, m_configManager->getString("dht.dataDirectory", "data"));

        // Set bootstrap nodes from config
        if (m_configManager->hasKey("dht.bootstrapNodes")) {
            std::vector<std::string> bootstrapNodes = m_configManager->getStringArray("dht.bootstrapNodes");
            if (!bootstrapNodes.empty()) {
                dhtConfig.setBootstrapNodes(bootstrapNodes);
            }
        }

        // Apply additional DHT configuration from config file
        dhtConfig.setKBucketSize(static_cast<size_t>(m_configManager->getInt("dht.kBucketSize", 16)));
        dhtConfig.setAlpha(static_cast<size_t>(m_configManager->getInt("dht.alpha", 3)));
        dhtConfig.setMaxResults(static_cast<size_t>(m_configManager->getInt("dht.maxResults", 8)));
        dhtConfig.setTokenRotationInterval(m_configManager->getInt("dht.tokenRotationInterval", 300));
        dhtConfig.setBucketRefreshInterval(m_configManager->getInt("dht.bucketRefreshInterval", 60));
        dhtConfig.setMaxIterations(static_cast<size_t>(m_configManager->getInt("dht.maxIterations", 10)));
        dhtConfig.setMaxQueries(static_cast<size_t>(m_configManager->getInt("dht.maxQueries", 100)));

        // Create persistence manager
        std::string configDir = m_configManager->getString("dht.dataDirectory", "data");
        m_persistenceManager = dht::PersistenceManager::getInstance(configDir);

        // Load or generate node ID
        types::NodeID nodeID;
        if (!m_persistenceManager->loadNodeID(nodeID)) {
            // Generate a new node ID if none exists
            nodeID = types::generateRandomNodeID();
            m_persistenceManager->saveNodeID(nodeID);
        }

        // Create and start DHT node
        m_dhtNode = std::make_shared<dht::DHTNode>(dhtConfig, nodeID);
        if (!m_dhtNode->start()) {
            unified_event::logError("Core.ApplicationController", "Failed to start DHT node");
            return false;
        }

        // Start the persistence manager
        auto routingTable = m_dhtNode->getRoutingTable();
        auto peerStorage = m_dhtNode->getPeerStorage();
        if (!m_persistenceManager->start(routingTable, peerStorage)) {
            unified_event::logError("Core.ApplicationController", "Failed to start persistence manager");
        }

        // Get routing manager
        auto routingManager = m_dhtNode->getRoutingManager();

        // Get statistics service
        auto statsService = dht::services::StatisticsService::getInstance();

        // Start the metadata acquisition manager
        m_metadataManager = bittorrent::metadata::MetadataAcquisitionManager::getInstance(peerStorage);
        if (!m_metadataManager->start()) {
            unified_event::logError("Core.ApplicationController", "Failed to start metadata acquisition manager");
        } else {
            unified_event::logDebug("Core.ApplicationController", "Metadata acquisition manager started");

            // Trigger metadata acquisition for existing InfoHashes
            if (peerStorage) {
                auto infoHashes = peerStorage->getAllInfoHashes();
                unified_event::logDebug("Core.ApplicationController", "Triggering metadata acquisition for " +
                                      std::to_string(infoHashes.size()) + " existing InfoHashes");

                for (const auto& infoHash : infoHashes) {
                    m_metadataManager->acquireMetadata(infoHash);
                }
            }
        }

        // Create and start web server
        m_webServer = std::make_shared<web::WebServer>(
            m_webRoot,
            m_webPort,
            statsService,
            routingManager,
            peerStorage,
            m_metadataManager
        );

        if (!m_webServer->start()) {
            unified_event::logError("Core.ApplicationController", "Failed to start web server");
        } else {
            unified_event::logInfo("Core.ApplicationController", "Web UI available at http://localhost:" + std::to_string(m_webPort));
        }

        // Start the task scheduler thread
        m_schedulerRunning = true;
        m_schedulerThread = std::thread(&ApplicationController::taskSchedulerThread, this);

        // Schedule recurring tasks
        scheduleRecurringTask("TitleUpdate", [this]() { updateTitle(); }, 1000);
        scheduleRecurringTask("ActivityCheck", [this]() {
            if (!m_systemSleeping && m_dhtNode && m_dhtNode->isRunning()) {
                auto activityRoutingManager = m_dhtNode->getRoutingManager();
                if (activityRoutingManager) {
                    size_t nodeCount = activityRoutingManager->getNodeCount();
                    unified_event::logDebug("Core.ApplicationController", "Activity check: " +
                                          std::to_string(nodeCount) + " nodes in routing table");
                }
            }
        }, 30000);

        m_running = true;
        unified_event::logInfo("Core.ApplicationController", "Application started successfully");
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Start failed: " + std::string(e.what()));
        return false;
    }
}

bool ApplicationController::stop() {
    if (!m_running) {
        return true;
    }

    try {
        unified_event::logInfo("Core.ApplicationController", "Stopping application");

        // Stop the task scheduler
        m_schedulerRunning = false;
        m_tasksCondition.notify_all();
        if (m_schedulerThread.joinable()) {
            m_schedulerThread.join();
        }

        // Stop all components in reverse order of creation
        if (m_webServer) {
            m_webServer->stop();
            m_webServer.reset();
        }

        if (m_metadataManager) {
            m_metadataManager->stop();
            m_metadataManager.reset();
        }

        if (m_persistenceManager && m_dhtNode) {
            // Save the node ID before stopping
            m_persistenceManager->saveNodeID(m_dhtNode->getNodeID());
            m_persistenceManager->stop();
        } else if (m_persistenceManager) {
            m_persistenceManager->stop();
        }

        if (m_dhtNode) {
            m_dhtNode->stop();
            m_dhtNode.reset();
        }

        if (m_udpServer) {
            m_udpServer->stop();
            m_udpServer.reset();
        }

        m_running = false;
        m_paused = false;
        unified_event::logInfo("Core.ApplicationController", "Application stopped successfully");
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Stop failed: " + std::string(e.what()));
        return false;
    }
}

bool ApplicationController::pause() {
    if (!m_running) {
        unified_event::logWarning("Core.ApplicationController", "Cannot pause: application not running");
        return false;
    }

    if (m_paused) {
        unified_event::logWarning("Core.ApplicationController", "Application already paused");
        return true;
    }

    try {
        unified_event::logInfo("Core.ApplicationController", "Pausing application");

        // Pause components that support it
        // For now, we'll just set the flag
        m_paused = true;

        unified_event::logInfo("Core.ApplicationController", "Application paused successfully");
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Pause failed: " + std::string(e.what()));
        return false;
    }
}

bool ApplicationController::resume() {
    if (!m_running) {
        unified_event::logWarning("Core.ApplicationController", "Cannot resume: application not running");
        return false;
    }

    if (!m_paused) {
        unified_event::logWarning("Core.ApplicationController", "Application not paused");
        return true;
    }

    try {
        unified_event::logInfo("Core.ApplicationController", "Resuming application");

        // Resume components that were paused
        // For now, we'll just clear the flag
        m_paused = false;

        unified_event::logInfo("Core.ApplicationController", "Application resumed successfully");
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Resume failed: " + std::string(e.what()));
        return false;
    }
}

bool ApplicationController::isRunning() const {
    return m_running;
}

bool ApplicationController::isPaused() const {
    return m_paused;
}

std::shared_ptr<dht::DHTNode> ApplicationController::getDHTNode() const {
    return m_dhtNode;
}

std::shared_ptr<web::WebServer> ApplicationController::getWebServer() const {
    return m_webServer;
}

std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> ApplicationController::getMetadataManager() const {
    return m_metadataManager;
}

std::string ApplicationController::scheduleTask(
    const std::string& name,
    std::function<void()> task,
    uint64_t delayMs,
    TaskPriority priority) {

    auto taskId = generateTaskId();
    auto scheduledTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    auto taskInfo = std::make_shared<TaskInfo>();
    taskInfo->id = taskId;
    taskInfo->name = name;
    taskInfo->priority = priority;
    taskInfo->scheduledTime = scheduledTime;
    taskInfo->interval = std::chrono::milliseconds(0); // One-time task
    taskInfo->recurring = false;
    taskInfo->task = task;
    taskInfo->cancelled = false;

    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.push_back(taskInfo);

        // Sort tasks by scheduled time and priority
        std::sort(m_tasks.begin(), m_tasks.end(), [](const auto& a, const auto& b) {
            if (a->scheduledTime == b->scheduledTime) {
                return static_cast<int>(a->priority) > static_cast<int>(b->priority);
            }
            return a->scheduledTime < b->scheduledTime;
        });
    }

    // Notify the scheduler thread
    m_tasksCondition.notify_one();

    return taskId;
}

std::string ApplicationController::scheduleRecurringTask(
    const std::string& name,
    std::function<void()> task,
    uint64_t intervalMs,
    uint64_t delayMs,
    TaskPriority priority) {

    auto taskId = generateTaskId();
    auto scheduledTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);

    auto taskInfo = std::make_shared<TaskInfo>();
    taskInfo->id = taskId;
    taskInfo->name = name;
    taskInfo->priority = priority;
    taskInfo->scheduledTime = scheduledTime;
    taskInfo->interval = std::chrono::milliseconds(intervalMs);
    taskInfo->recurring = true;
    taskInfo->task = task;
    taskInfo->cancelled = false;

    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.push_back(taskInfo);

        // Sort tasks by scheduled time and priority
        std::sort(m_tasks.begin(), m_tasks.end(), [](const auto& a, const auto& b) {
            if (a->scheduledTime == b->scheduledTime) {
                return static_cast<int>(a->priority) > static_cast<int>(b->priority);
            }
            return a->scheduledTime < b->scheduledTime;
        });
    }

    // Notify the scheduler thread
    m_tasksCondition.notify_one();

    return taskId;
}

bool ApplicationController::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_tasksMutex);

    for (auto& task : m_tasks) {
        if (task->id == taskId) {
            task->cancelled = true;
            return true;
        }
    }

    return false;
}

void ApplicationController::handleSystemSleep() {
    unified_event::logInfo("Core.ApplicationController", "System is going to sleep");
    m_systemSleeping.store(true, std::memory_order_release);

    // Pause non-critical operations
    pause();
}

void ApplicationController::handleSystemWake() {
    unified_event::logInfo("Core.ApplicationController", "System has woken up");
    m_systemSleeping.store(false, std::memory_order_release);

    // Resume operations
    resume();

    // Trigger a refresh of the routing table
    if (m_dhtNode && m_running) {
        auto dhtRoutingManager = m_dhtNode->getRoutingManager();
        if (dhtRoutingManager) {
            dhtRoutingManager->refreshAllBuckets();
        }
    }
}

void ApplicationController::updateTitle() {
    if (!m_running || m_systemSleeping) {
        return;
    }

    try {
        // Get statistics
        std::string title = "BitScrape";

        if (m_dhtNode) {
            auto routingManager = m_dhtNode->getRoutingManager();
            auto peerStorage = m_dhtNode->getPeerStorage();
            auto statsService = dht::services::StatisticsService::getInstance();

            if (routingManager && peerStorage && statsService) {
                size_t nodeCount = routingManager->getNodeCount();
                size_t infoHashCount = peerStorage->getInfoHashCount();
                size_t peerCount = peerStorage->getTotalPeerCount();

                // Get message statistics
                size_t messagesReceived = statsService->getMessagesReceived();
                size_t messagesSent = statsService->getMessagesSent();

                // Format network stats
                std::stringstream ss;
                ss << "BitScrape | Nodes: " << nodeCount
                   << " | InfoHashes: " << infoHashCount
                   << " | Peers: " << peerCount
                   << " | In: " << messagesReceived << " msgs"
                   << " | Out: " << messagesSent << " msgs";

                title = ss.str();
            }
        }

        // Set console title
#ifdef _WIN32
        SetConsoleTitleA(title.c_str());
#else
        std::cout << "\033]0;" << title << "\007";
#endif
    } catch (const std::exception& e) {
        unified_event::logError("Core.ApplicationController", "Failed to update title: " + std::string(e.what()));
    }
}

void ApplicationController::initializeThreadPool(size_t threadCount) {
    // Get thread count from config if available
    if (m_configManager) {
        threadCount = m_configManager->getInt("application.threadPoolSize", threadCount);
    }

    // Ensure at least 2 threads
    threadCount = std::max(threadCount, static_cast<size_t>(2));

    // Create the thread pool
    m_threadPool = std::make_shared<utility::thread::ThreadPool>(threadCount);

    unified_event::logDebug("Core.ApplicationController", "Thread pool initialized with " +
                          std::to_string(threadCount) + " threads");
}

void ApplicationController::taskSchedulerThread() {
    unified_event::logDebug("Core.ApplicationController", "Task scheduler thread started");

    while (m_schedulerRunning) {
        std::shared_ptr<TaskInfo> nextTask = nullptr;

        {
            std::unique_lock<std::mutex> lock(m_tasksMutex);

            // Wait for a task or stop signal
            auto waitUntil = std::chrono::steady_clock::now() + std::chrono::seconds(1);

            if (!m_tasks.empty()) {
                waitUntil = m_tasks.front()->scheduledTime;
            }

            m_tasksCondition.wait_until(lock, waitUntil, [this, waitUntil]() {
                return !m_schedulerRunning ||
                       (!m_tasks.empty() && m_tasks.front()->scheduledTime <= std::chrono::steady_clock::now());
            });

            if (!m_schedulerRunning) {
                break;
            }

            // Get the next task if it's time to execute
            if (!m_tasks.empty() && m_tasks.front()->scheduledTime <= std::chrono::steady_clock::now()) {
                nextTask = m_tasks.front();
                m_tasks.erase(m_tasks.begin());
            }
        }

        // Execute the task if we have one
        if (nextTask && !nextTask->cancelled) {
            try {
                // Execute the task asynchronously
                m_threadPool->enqueue([nextTask]() {
                    try {
                        if (!nextTask->cancelled) {
                            nextTask->task();
                        }
                    } catch (const std::exception& e) {
                        unified_event::logError("Core.ApplicationController",
                                              "Task '" + nextTask->name + "' failed: " + std::string(e.what()));
                    }
                });

                // If it's a recurring task, reschedule it
                if (nextTask->recurring) {
                    nextTask->scheduledTime = std::chrono::steady_clock::now() + nextTask->interval;

                    std::lock_guard<std::mutex> lock(m_tasksMutex);
                    m_tasks.push_back(nextTask);

                    // Sort tasks by scheduled time and priority
                    std::sort(m_tasks.begin(), m_tasks.end(), [](const auto& a, const auto& b) {
                        if (a->scheduledTime == b->scheduledTime) {
                            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
                        }
                        return a->scheduledTime < b->scheduledTime;
                    });
                }
            } catch (const std::exception& e) {
                unified_event::logError("Core.ApplicationController",
                                      "Failed to schedule task '" + nextTask->name + "': " + std::string(e.what()));
            }
        }
    }

    unified_event::logDebug("Core.ApplicationController", "Task scheduler thread stopped");
}

std::string ApplicationController::generateTaskId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* digits = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else if (i == 14) {
            uuid += '4';
        } else if (i == 19) {
            uuid += digits[(dis(gen) & 0x3) | 0x8];
        } else {
            uuid += digits[dis(gen)];
        }
    }

    return uuid;
}

void ApplicationController::loadConfiguration() {
    if (!m_configManager) {
        return;
    }

    // Load web server settings
    m_webPort = static_cast<uint16_t>(m_configManager->getInt("web.port", 8080));

    // Load DHT settings
    m_dhtPort = static_cast<uint16_t>(m_configManager->getInt("dht.port", 6881));

    // Load thread pool settings
    size_t threadCount = m_configManager->getInt("application.threadPoolSize", std::thread::hardware_concurrency());
    threadCount = std::max(threadCount, static_cast<size_t>(2)); // Ensure at least 2 threads

    // Reinitialize thread pool if needed
    if (m_threadPool && m_threadPool->getThreadCount() != threadCount) {
        unified_event::logInfo("Core.ApplicationController", "Reinitializing thread pool with " +
                             std::to_string(threadCount) + " threads");
        m_threadPool = std::make_shared<utility::thread::ThreadPool>(threadCount);
    }
}

void ApplicationController::registerEventHandlers() {
    auto eventBus = unified_event::EventBus::getInstance();

    // Register for system events
    eventBus->subscribe(unified_event::EventType::SystemStarted, [this](const std::shared_ptr<unified_event::Event>& event) {
        unified_event::logInfo("Core.ApplicationController", "System started event received");
    });

    eventBus->subscribe(unified_event::EventType::SystemStopped, [this](const std::shared_ptr<unified_event::Event>& event) {
        unified_event::logInfo("Core.ApplicationController", "System stopped event received");
    });

    // Register for node events
    eventBus->subscribe(unified_event::EventType::NodeDiscovered, [this](const std::shared_ptr<unified_event::Event>& event) {
        unified_event::logDebug("Core.ApplicationController", "Node discovered event received");
    });

    // Register for peer events
    eventBus->subscribe(unified_event::EventType::PeerDiscovered, [this](const std::shared_ptr<unified_event::Event>& event) {
        unified_event::logDebug("Core.ApplicationController", "Peer discovered event received");

        // If we have a metadata manager, we could trigger metadata acquisition for the InfoHash
        if (m_metadataManager) {
            auto peerEvent = std::dynamic_pointer_cast<unified_event::PeerDiscoveredEvent>(event);
            if (peerEvent) {
                auto infoHashPtr = peerEvent->getProperty<types::InfoHash>("infoHash");
                if (infoHashPtr) {
                    unified_event::logDebug("Core.ApplicationController", "Triggering metadata acquisition for InfoHash");
                    m_metadataManager->acquireMetadata(*infoHashPtr);
                }
            }
        }
    });

    // Register for error events
    eventBus->subscribe(unified_event::EventType::SystemError, [this](const std::shared_ptr<unified_event::Event>& event) {
        unified_event::logError("Core.ApplicationController", "System error event received");

        // Get error details if available
        std::string errorMessage = "Unknown error";
        auto messagePtr = event->getProperty<std::string>("message");
        if (messagePtr) {
            errorMessage = *messagePtr;
        }

        unified_event::logError("Core.ApplicationController", "Error details: " + errorMessage);
    });

    // Register for custom events
    eventBus->subscribe(unified_event::EventType::Custom, [this](const std::shared_ptr<unified_event::Event>& event) {
        auto customEvent = std::dynamic_pointer_cast<unified_event::CustomEvent>(event);
        if (customEvent) {
            std::string message = customEvent->getMessage();
            unified_event::logDebug("Core.ApplicationController", "Custom event received: " + message);

            // Handle specific custom events
            if (message == "CONFIG_CHANGED") {
                unified_event::logInfo("Core.ApplicationController", "Configuration changed, reloading settings");
                loadConfiguration();
            } else if (message == "SYSTEM_SLEEP") {
                handleSystemSleep();
            } else if (message == "SYSTEM_WAKE") {
                handleSystemWake();
            }
        }
    });
}

} // namespace dht_hunter::core
