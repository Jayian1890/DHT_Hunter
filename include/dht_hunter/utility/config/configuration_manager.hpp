#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <any>
#include <optional>
#include <filesystem>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace dht_hunter::utility::config {

/**
 * @brief Callback function type for configuration change events
 * @param key The key of the changed configuration value (empty string for any change)
 */
using ConfigChangeCallback = std::function<void(const std::string&)>;

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
     * @brief Delete copy constructor and assignment operator
     */
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    /**
     * @brief Loads the configuration from the specified file
     * @param configFilePath Path to the configuration file
     * @return True if the configuration was loaded successfully, false otherwise
     */
    bool loadConfiguration(const std::string& configFilePath);

    /**
     * @brief Reloads the configuration from the current file
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
     * @brief Gets a string value from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value, or the default value if not found
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Gets an integer value from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value, or the default value if not found
     */
    int getInt(const std::string& key, int defaultValue = 0) const;

    /**
     * @brief Gets a boolean value from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value, or the default value if not found
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Gets a double value from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value, or the default value if not found
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

    /**
     * @brief Gets a string array from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @return The configuration value as a vector of strings, or an empty vector if not found
     */
    std::vector<std::string> getStringArray(const std::string& key) const;

    /**
     * @brief Gets an integer array from the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @return The configuration value as a vector of integers, or an empty vector if not found
     */
    std::vector<int> getIntArray(const std::string& key) const;

    /**
     * @brief Sets a string value in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setString(const std::string& key, const std::string& value);

    /**
     * @brief Sets an integer value in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setInt(const std::string& key, int value);

    /**
     * @brief Sets a boolean value in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setBool(const std::string& key, bool value);

    /**
     * @brief Sets a double value in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setDouble(const std::string& key, double value);

    /**
     * @brief Sets a string array in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setStringArray(const std::string& key, const std::vector<std::string>& value);

    /**
     * @brief Sets an integer array in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @param value The value to set
     */
    void setIntArray(const std::string& key, const std::vector<int>& value);

    /**
     * @brief Checks if a key exists in the configuration
     * @param key The configuration key (using dot notation for nested objects)
     * @return True if the key exists, false otherwise
     */
    bool hasKey(const std::string& key) const;

    /**
     * @brief Gets the current configuration file path
     * @return The current configuration file path
     */
    std::string getConfigFilePath() const;

    /**
     * @brief Validates the configuration
     * @return True if the configuration is valid, false otherwise
     */
    bool validateConfiguration() const;

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
     * @brief Unregisters a callback for configuration change events
     * @param callbackId The unique identifier returned by registerChangeCallback
     * @return True if the callback was unregistered successfully, false otherwise
     */
    bool unregisterChangeCallback(int callbackId);

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param configFilePath Path to the configuration file
     */
    explicit ConfigurationManager(const std::string& configFilePath);

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

    // Static instance for singleton pattern
    static std::shared_ptr<ConfigurationManager> s_instance;
    static std::mutex s_instanceMutex;

    /**
     * @brief Notifies all registered callbacks of a configuration change
     * @param key The key of the changed configuration value (empty string for any change)
     */
    void notifyChangeCallbacks(const std::string& key = "");

    /**
     * @brief File watcher thread function
     */
    void fileWatcherThread();

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

} // namespace dht_hunter::utility::config
