#pragma once

/**
 * @file config_utils.hpp
 * @brief Configuration management utilities for the BitScrape project
 *
 * This file consolidates configuration management functionality from:
 * - configuration_manager.hpp (Configuration management)
 */

// Standard C++ libraries
#include <string>
#include <memory>
#include <mutex>
#include <any>
#include <optional>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <thread>
#include <condition_variable>
#include <atomic>

// Project includes
#include "utils/misc_utils.hpp"

namespace dht_hunter::utility {

//
// Configuration Management
//
namespace config {

/**
 * @brief Callback function type for configuration changes
 */
using ConfigChangeCallback = std::function<void(const std::string&, const std::any&)>;

/**
 * @brief Manager for application configuration
 *
 * This class is responsible for loading, parsing, and providing access to
 * application configuration settings from a JSON file.
 */
class ConfigurationManager {
public:
    /**
     * @brief Gets the singleton instance of the configuration manager
     * @param configFilePath Optional path to the configuration file
     * @return The singleton instance
     */
    static std::shared_ptr<ConfigurationManager> getInstance(const std::string& configFilePath = "");

    /**
     * @brief Destructor
     */
    ~ConfigurationManager();

    /**
     * @brief Loads configuration from a file
     * @param configFilePath Path to the configuration file
     * @return True if the configuration was loaded successfully, false otherwise
     */
    bool loadConfiguration(const std::string& configFilePath);

    /**
     * @brief Reloads the current configuration file
     * @return True if the configuration was reloaded successfully, false otherwise
     */
    bool reloadConfiguration();

    /**
     * @brief Saves the current configuration to the specified file
     * @param configFilePath Path to the configuration file (if empty, uses the current file)
     * @return True if the configuration was saved successfully, false otherwise
     */
    bool saveConfiguration(const std::string& configFilePath = "");

    /**
     * @brief Generates a default configuration file
     * @param configFilePath Path to the configuration file
     * @return True if the configuration was generated successfully, false otherwise
     */
    static bool generateDefaultConfiguration(const std::string& configFilePath);

    /**
     * @brief Gets a value from the configuration
     * @param key The key to look up
     * @param defaultValue The default value to return if the key is not found
     * @return The value if found, or the default value if not found
     */
    template <typename T>
    T getValue(const std::string& key, const T& defaultValue) const {
        auto result = getValueOptional<T>(key);
        if (result.has_value()) {
            return result.value();
        }
        return defaultValue;
    }

    /**
     * @brief Gets a value from the configuration
     * @param key The key to look up
     * @return The value if found, or std::nullopt if not found
     */
    template <typename T>
    std::optional<T> getValueOptional(const std::string& key) const {
        auto keyPath = splitKeyPath(key);
        auto result = getValueFromPath(keyPath);
        if (!result.has_value()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(result.value());
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /**
     * @brief Sets a value in the configuration
     * @param key The key to set
     * @param value The value to set
     * @return True if the value was set successfully, false otherwise
     */
    template <typename T>
    bool setValue(const std::string& key, const T& value) {
        auto keyPath = splitKeyPath(key);
        return setValueAtPath(keyPath, std::any(value));
    }

    /**
     * @brief Checks if a key exists in the configuration
     * @param key The key to check
     * @return True if the key exists, false otherwise
     */
    bool hasKey(const std::string& key) const;

    /**
     * @brief Gets all keys in the configuration
     * @return A vector of all keys
     */
    std::vector<std::string> getKeys() const;

    /**
     * @brief Enables hot-reloading of the configuration file
     * @param enabled Whether to enable hot-reloading
     * @param checkIntervalMs The interval in milliseconds to check for file changes
     * @return True if hot-reloading was enabled successfully, false otherwise
     */
    bool enableHotReloading(bool enabled, int checkIntervalMs = 1000);

    /**
     * @brief Registers a callback for configuration change events
     * @param callback The callback function to register
     * @param key The key to watch for changes (empty string for any change)
     * @return A unique identifier for the callback registration
     */
    int registerChangeCallback(const ConfigChangeCallback& callback, const std::string& key = "");

    /**
     * @brief Unregisters a callback
     * @param callbackId The callback identifier returned by registerChangeCallback
     * @return True if the callback was unregistered successfully, false otherwise
     */
    bool unregisterChangeCallback(int callbackId);

private:
    /**
     * @brief Constructor
     * @param configFilePath Path to the configuration file
     */
    ConfigurationManager(const std::string& configFilePath);

    /**
     * @brief File watcher thread function
     */
    void fileWatcherThread();

    /**
     * @brief Notifies all registered callbacks of a configuration change
     * @param key The key of the changed configuration value (empty string for any change)
     */
    void notifyChangeCallbacks(const std::string& key = "");

    /**
     * @brief Gets a value from the configuration using a key path
     * @param keyPath The key path split into components
     * @return The value if found, or std::nullopt if not found
     */
    std::optional<std::any> getValueFromPath(const std::vector<std::string>& keyPath) const;

    /**
     * @brief Sets a value in the configuration using a key path
     * @param keyPath The key path split into components
     * @param value The value to set
     * @return True if the value was set successfully, false otherwise
     */
    bool setValueAtPath(const std::vector<std::string>& keyPath, const std::any& value);

    /**
     * @brief Splits a key into path components
     * @param key The key to split
     * @return The key split into path components
     */
    static std::vector<std::string> splitKeyPath(const std::string& key);

    // Singleton instance
    static std::shared_ptr<ConfigurationManager> s_instance;
    static std::mutex s_instanceMutex;

    // Configuration data
    std::any m_configRoot;
    std::string m_configFilePath;
    mutable std::mutex m_configMutex;
    bool m_configLoaded;

    // Hot-reloading
    std::atomic<bool> m_hotReloadingEnabled;
    std::atomic<bool> m_fileWatcherRunning;
    std::thread m_fileWatcherThread;
    std::condition_variable m_fileWatcherCondition;
    std::mutex m_fileWatcherMutex;
    int m_fileWatcherIntervalMs;
    std::filesystem::file_time_type m_lastModifiedTime;

    // Change callbacks
    struct CallbackInfo {
        ConfigChangeCallback callback;
        std::string key;
    };
    std::unordered_map<int, CallbackInfo> m_changeCallbacks;
    std::mutex m_callbacksMutex;
    int m_nextCallbackId;
};

} // namespace config

} // namespace dht_hunter::utility
