#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif

// Include thread utilities for the global shutdown flag
#include "dht_hunter/utility/thread/thread_utils.hpp"

// Include the application controller
#include "dht_hunter/core/application_controller.hpp"

// Include unified event system
#include "dht_hunter/unified_event/unified_event.hpp"

// Shared pointer to the application controller
std::shared_ptr<dht_hunter::core::ApplicationController> g_appController;

// Web server port (default value)
uint16_t webPort = 8080;

#ifdef __APPLE__
// IOKit sleep/wake notification callback
IONotificationPortRef g_notifyPortRef = nullptr;
io_object_t g_sleepNotifierRef = IO_OBJECT_NULL;
io_object_t g_wakeNotifierRef = IO_OBJECT_NULL;
io_connect_t g_rootPowerDomain = IO_OBJECT_NULL;

// Sleep notification callback
void sleepCallBack(void* /*refCon*/, io_service_t /*service*/, natural_t messageType, void* messageArgument) {
    if (messageType == kIOMessageSystemWillSleep) {
        std::cout << "System is going to sleep..." << std::endl;
        dht_hunter::unified_event::logInfo("Main", "System is going to sleep");

        // Notify the application controller
        if (g_appController) {
            g_appController->handleSystemSleep();
        }

        // Acknowledge the sleep notification
        IOAllowPowerChange(g_rootPowerDomain, reinterpret_cast<intptr_t>(messageArgument));
    }
}

// Wake notification callback
void wakeCallBack(void* /*refCon*/, io_service_t /*service*/, natural_t messageType, void* /*messageArgument*/) {
    if (messageType == kIOMessageSystemHasPoweredOn) {
        std::cout << "System has woken up..." << std::endl;
        dht_hunter::unified_event::logInfo("Main", "System has woken up");

        // Notify the application controller
        if (g_appController) {
            g_appController->handleSystemWake();
        }
    }
}

// Register for sleep/wake notifications
bool registerSleepWakeNotifications() {
    // Create a notification port
    g_notifyPortRef = IONotificationPortCreate(kIOMainPortDefault);
    if (!g_notifyPortRef) {
        std::cerr << "Failed to create notification port" << std::endl;
        return false;
    }

    // Get a run loop source for the notification port
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(g_notifyPortRef);

    // Add the run loop source to the current run loop
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);

    // Register for sleep notifications
    g_rootPowerDomain = IORegisterForSystemPower(nullptr, &g_notifyPortRef, sleepCallBack, &g_sleepNotifierRef);
    if (!g_rootPowerDomain) {
        std::cerr << "Failed to register for system power notifications" << std::endl;
        return false;
    }

    // Register for wake notifications - we don't need a separate registration
    // as the same callback will handle both sleep and wake events
    g_wakeNotifierRef = g_sleepNotifierRef;

    return true;
}

// Unregister sleep/wake notifications
void unregisterSleepWakeNotifications() {
    if (g_sleepNotifierRef != IO_OBJECT_NULL) {
        IODeregisterForSystemPower(&g_sleepNotifierRef);
        g_sleepNotifierRef = IO_OBJECT_NULL;
    }

    if (g_notifyPortRef) {
        IONotificationPortDestroy(g_notifyPortRef);
        g_notifyPortRef = nullptr;
    }

    if (g_rootPowerDomain != IO_OBJECT_NULL) {
        IOServiceClose(g_rootPowerDomain);
        g_rootPowerDomain = IO_OBJECT_NULL;
    }
}
#endif

/**
 * Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signalHandler(const int signal) {
    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
    dht_hunter::unified_event::logInfo("Main", "Received signal " + std::to_string(signal) + ", shutting down gracefully");

    // Set the global shutdown flag to prevent new lock acquisitions
    dht_hunter::utility::thread::g_shuttingDown.store(true, std::memory_order_release);

    // Unregister sleep/wake notifications on macOS
#ifdef __APPLE__
    unregisterSleepWakeNotifications();
#endif

    // Stop the application controller
    if (g_appController) {
        g_appController->stop();
    }
}

/**
 * Main entry point for the DHT Hunter application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit code (0 for success, non-zero for error)
 */
int main(int argc, char* argv[]) {
    std::string configDir;
    std::string configFile;
    std::string webRoot = "web";
    std::string logLevel = "";
    bool generateDefaultConfig = false;

#ifdef _WIN32
    // Windows
    const char* homeDir = std::getenv("USERPROFILE");
#else
    // Unix/Linux/macOS
    const char* homeDir = std::getenv("HOME");
#endif

    if (homeDir) {
        configDir = std::string(homeDir) + "/bitscrape";
    } else {
        configDir = "config";
    }

    // Default config file path
    configFile = configDir + "/bitscrape.json";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config-dir" || arg == "-c") {
            if (i + 1 < argc) {
                configDir = argv[++i];
                // Update config file path if it wasn't explicitly set
                if (configFile == configDir + "/bitscrape.json") {
                    configFile = configDir + "/bitscrape.json";
                }
            } else {
                std::cerr << "Error: --config-dir requires a directory path" << std::endl;
                return 1;
            }
        } else if (arg == "--config-file" || arg == "-f") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: --config-file requires a file path" << std::endl;
                return 1;
            }
        } else if (arg == "--generate-config" || arg == "-g") {
            generateDefaultConfig = true;
        } else if (arg == "--web-root" || arg == "-w") {
            if (i + 1 < argc) {
                webRoot = argv[++i];
            } else {
                std::cerr << "Error: --web-root requires a directory path" << std::endl;
                return 1;
            }
        } else if (arg == "--web-port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    webPort = static_cast<uint16_t>(std::stoi(argv[++i]));
                } catch (const std::exception& e) {
                    std::cerr << "Error: --web-port requires a valid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --web-port requires a port number" << std::endl;
                return 1;
            }
        } else if (arg == "--log-level" || arg == "-l") {
            if (i + 1 < argc) {
                logLevel = argv[++i];
                // Convert to uppercase for consistency
                std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(), ::toupper);
                // Validate log level
                if (logLevel != "TRACE" && logLevel != "DEBUG" && logLevel != "INFO" &&
                    logLevel != "WARNING" && logLevel != "ERROR" && logLevel != "CRITICAL") {
                    std::cerr << "Error: --log-level must be one of: TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --log-level requires a level (TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL)" << std::endl;
                return 1;
            }
        }
    }

    // Create config directory if it doesn't exist
    std::filesystem::path configPath(configDir);
    if (!std::filesystem::exists(configPath)) {
        std::filesystem::create_directories(configPath);
    }

    // Initialize the unified event system with default settings
    // (These will be overridden by the ApplicationController)
    dht_hunter::unified_event::initializeEventSystem(true, true, true, false);

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request
    std::signal(SIGABRT, signalHandler);  // Abort signal

    // Register for sleep/wake notifications on macOS
#ifdef __APPLE__
    if (!registerSleepWakeNotifications()) {
        dht_hunter::unified_event::logWarning("Main", "Failed to register for sleep/wake notifications");
    } else {
        dht_hunter::unified_event::logDebug("Main", "Registered for sleep/wake notifications");
    }
#endif

    // Get the application controller instance
    g_appController = dht_hunter::core::ApplicationController::getInstance();

    // Initialize the application controller with web port and log level
    if (!g_appController->initialize(configFile, webRoot, webPort, logLevel)) {
        dht_hunter::unified_event::logError("Main", "Failed to initialize application controller");
        return 1;
    }

    // Generate default config if requested
    if (generateDefaultConfig) {
        if (!g_appController->generateDefaultConfig()) {
            dht_hunter::unified_event::logError("Main", "Failed to generate default configuration");
            return 1;
        }
        dht_hunter::unified_event::logInfo("Main", "Default configuration generated at: " + configFile);
    }

    // Start the application
    if (!g_appController->start()) {
        dht_hunter::unified_event::logError("Main", "Failed to start application");
        return 1;
    }

    dht_hunter::unified_event::logInfo("Main", "Application started successfully");

    // Main loop - keep running until signaled to stop
    while (g_appController->isRunning()) {
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Application has been stopped, clean up
    dht_hunter::unified_event::logInfo("Main", "Application stopped");

    // Unregister sleep/wake notifications on macOS
#ifdef __APPLE__
    unregisterSleepWakeNotifications();
#endif

    // Shutdown the unified event system
    dht_hunter::unified_event::shutdownEventSystem();

    return 0;
}
