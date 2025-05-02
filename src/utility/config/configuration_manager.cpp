#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/json/json.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace dht_hunter::utility::config {

namespace fs = std::filesystem;

// Initialize static members
std::shared_ptr<ConfigurationManager> ConfigurationManager::s_instance = nullptr;
std::mutex ConfigurationManager::s_instanceMutex;

std::shared_ptr<ConfigurationManager> ConfigurationManager::getInstance(const std::string& configFilePath) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&configFilePath]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<ConfigurationManager>(new ConfigurationManager(configFilePath));
            } else if (!configFilePath.empty() && configFilePath != s_instance->getConfigFilePath()) {
                // If a different config file is specified, reload with the new file
                s_instance->loadConfiguration(configFilePath);
            }
            return s_instance;
        }, "ConfigurationManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return nullptr;
    }
}

ConfigurationManager::ConfigurationManager(const std::string& configFilePath)
    : m_configFilePath(configFilePath),
      m_configLoaded(false),
      m_hotReloadingEnabled(false),
      m_fileWatcherRunning(false),
      m_fileWatcherIntervalMs(1000),
      m_nextCallbackId(1) {

    // Create an empty JSON object as the root
    m_configRoot = json::JsonValue(json::JsonValue::createObject());

    // If a config file path was provided, try to load it
    if (!configFilePath.empty()) {
        loadConfiguration(configFilePath);

        // Store the last modified time of the file
        try {
            if (fs::exists(configFilePath)) {
                m_lastModifiedTime = fs::last_write_time(configFilePath);
            }
        } catch (const std::exception& e) {
            unified_event::logWarning("ConfigurationManager", "Failed to get last modified time of configuration file: " + std::string(e.what()));
        }
    }
}

ConfigurationManager::~ConfigurationManager() {
    // Stop the file watcher thread if it's running
    enableHotReloading(false);

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "ConfigurationManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& configFilePath) {
    try {
        return utility::thread::withLock(m_configMutex, [this, &configFilePath]() {
            unified_event::logDebug("ConfigurationManager", "Loading configuration from file: " + configFilePath);

            // Check if the file exists
            if (!fs::exists(configFilePath)) {
                unified_event::logError("ConfigurationManager", "Configuration file does not exist: " + configFilePath);
                return false;
            }

            // Open the file
            std::ifstream file(configFilePath);
            if (!file.is_open()) {
                unified_event::logError("ConfigurationManager", "Failed to open configuration file: " + configFilePath);
                return false;
            }

            // Read the file contents
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string jsonStr = buffer.str();
            unified_event::logDebug("ConfigurationManager", "Read configuration file content, size: " + std::to_string(jsonStr.size()));

            // Parse the JSON
            auto jsonValue = json::JsonValue::parse(jsonStr);
            unified_event::logDebug("ConfigurationManager", "Parsed JSON configuration");

            // Check if the root is an object
            if (!jsonValue.isObject()) {
                unified_event::logError("ConfigurationManager", "Configuration root must be a JSON object");
                return false;
            }

            // Store the configuration
            auto oldRoot = m_configRoot;
            m_configRoot = jsonValue;
            m_configFilePath = configFilePath;
            m_configLoaded = true;
            unified_event::logDebug("ConfigurationManager", "Configuration loaded successfully");

            // Update the last modified time
            try {
                m_lastModifiedTime = fs::last_write_time(configFilePath);
            } catch (const std::exception& e) {
                unified_event::logWarning("ConfigurationManager", "Failed to get last modified time of configuration file: " + std::string(e.what()));
            }

            // Notify callbacks if this is a reload (not the initial load)
            if (oldRoot.has_value()) {
                notifyChangeCallbacks();
            }

            unified_event::logInfo("ConfigurationManager", "Successfully loaded configuration from " + configFilePath);
            return true;
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    } catch (const std::exception& e) {
        unified_event::logError("ConfigurationManager", "Exception while loading configuration: " + std::string(e.what()));
        return false;
    }
}

bool ConfigurationManager::reloadConfiguration() {
    if (m_configFilePath.empty()) {
        unified_event::logError("ConfigurationManager", "No configuration file path specified");
        return false;
    }
    return loadConfiguration(m_configFilePath);
}

bool ConfigurationManager::saveConfiguration(const std::string& configFilePath) {
    try {
        return utility::thread::withLock(m_configMutex, [this, &configFilePath]() {
            // Use the current file path if none is specified
            std::string filePath = configFilePath.empty() ? m_configFilePath : configFilePath;
            if (filePath.empty()) {
                unified_event::logError("ConfigurationManager", "No configuration file path specified");
                return false;
            }

            // Create the directory if it doesn't exist
            fs::path path(filePath);
            fs::path dir = path.parent_path();
            if (!dir.empty() && !fs::exists(dir)) {
                try {
                    fs::create_directories(dir);
                } catch (const std::exception& e) {
                    unified_event::logError("ConfigurationManager", "Failed to create directory: " + dir.string() + " - " + e.what());
                    return false;
                }
            }

            // Convert the configuration to a JSON string
            auto jsonValue = std::any_cast<std::shared_ptr<json::JsonValue>>(m_configRoot);
            std::string jsonStr = jsonValue->toString(true); // Pretty print

            // Write to the file
            std::ofstream file(filePath);
            if (!file.is_open()) {
                unified_event::logError("ConfigurationManager", "Failed to open configuration file for writing: " + filePath);
                return false;
            }

            file << jsonStr;
            file.close();

            // Update the file path if a new one was specified
            if (!configFilePath.empty()) {
                m_configFilePath = configFilePath;
            }

            unified_event::logInfo("ConfigurationManager", "Successfully saved configuration to " + filePath);
            return true;
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    } catch (const std::exception& e) {
        unified_event::logError("ConfigurationManager", "Exception while saving configuration: " + std::string(e.what()));
        return false;
    }
}

bool ConfigurationManager::generateDefaultConfiguration(const std::string& configFilePath) {
    try {
        // Create a default configuration
        json::JsonValue config(json::JsonValue::createObject());

        // General settings
        json::JsonValue generalConfig(json::JsonValue::createObject());
        generalConfig.getObject()->set("configDir", json::JsonValue("~/dht-hunter"));
        generalConfig.getObject()->set("logFile", json::JsonValue("dht_hunter.log"));
        generalConfig.getObject()->set("logLevel", json::JsonValue("info"));
        config.getObject()->set("general", generalConfig);

        // DHT settings
        json::JsonValue dhtConfig(json::JsonValue::createObject());
        dhtConfig.getObject()->set("port", json::JsonValue(6881));
        dhtConfig.getObject()->set("kBucketSize", json::JsonValue(16));
        dhtConfig.getObject()->set("alpha", json::JsonValue(3));
        dhtConfig.getObject()->set("maxResults", json::JsonValue(8));
        dhtConfig.getObject()->set("tokenRotationInterval", json::JsonValue(300));
        dhtConfig.getObject()->set("bucketRefreshInterval", json::JsonValue(60));
        dhtConfig.getObject()->set("maxIterations", json::JsonValue(10));
        dhtConfig.getObject()->set("maxQueries", json::JsonValue(100));

        // Bootstrap nodes
        json::JsonValue bootstrapNodes(json::JsonValue::createArray());
        bootstrapNodes.getArray()->add(json::JsonValue("dht.aelitis.com:6881"));
        bootstrapNodes.getArray()->add(json::JsonValue("dht.transmissionbt.com:6881"));
        bootstrapNodes.getArray()->add(json::JsonValue("dht.libtorrent.org:25401"));
        bootstrapNodes.getArray()->add(json::JsonValue("router.utorrent.com:6881"));
        dhtConfig.getObject()->set("bootstrapNodes", bootstrapNodes);

        config.getObject()->set("dht", dhtConfig);

        // Network settings
        json::JsonValue networkConfig(json::JsonValue::createObject());
        networkConfig.getObject()->set("transactionTimeout", json::JsonValue(30));
        networkConfig.getObject()->set("maxTransactions", json::JsonValue(1024));
        networkConfig.getObject()->set("mtuSize", json::JsonValue(1400));
        config.getObject()->set("network", networkConfig);

        // Web interface settings
        json::JsonValue webConfig(json::JsonValue::createObject());
        webConfig.getObject()->set("port", json::JsonValue(8080));
        webConfig.getObject()->set("webRoot", json::JsonValue("web"));
        config.getObject()->set("web", webConfig);

        // Persistence settings
        json::JsonValue persistenceConfig(json::JsonValue::createObject());
        persistenceConfig.getObject()->set("saveInterval", json::JsonValue(60)); // In minutes
        persistenceConfig.getObject()->set("routingTablePath", json::JsonValue("routing_table.dat"));
        persistenceConfig.getObject()->set("peerStoragePath", json::JsonValue("peer_storage.dat"));
        persistenceConfig.getObject()->set("metadataPath", json::JsonValue("metadata.dat"));
        persistenceConfig.getObject()->set("nodeIDPath", json::JsonValue("node_id.dat"));
        config.getObject()->set("persistence", persistenceConfig);

        // Crawler settings
        json::JsonValue crawlerConfig(json::JsonValue::createObject());
        crawlerConfig.getObject()->set("parallelCrawls", json::JsonValue(10));
        crawlerConfig.getObject()->set("refreshInterval", json::JsonValue(15));
        crawlerConfig.getObject()->set("maxNodes", json::JsonValue(1000000));
        crawlerConfig.getObject()->set("maxInfoHashes", json::JsonValue(1000000));
        crawlerConfig.getObject()->set("autoStart", json::JsonValue(true));
        config.getObject()->set("crawler", crawlerConfig);

        // Metadata acquisition settings
        json::JsonValue metadataConfig(json::JsonValue::createObject());
        metadataConfig.getObject()->set("processingInterval", json::JsonValue(5));
        metadataConfig.getObject()->set("maxConcurrentAcquisitions", json::JsonValue(5));
        metadataConfig.getObject()->set("acquisitionTimeout", json::JsonValue(60));
        metadataConfig.getObject()->set("maxRetryCount", json::JsonValue(3));
        metadataConfig.getObject()->set("retryDelayBase", json::JsonValue(300));
        config.getObject()->set("metadata", metadataConfig);

        // Event system settings
        json::JsonValue eventConfig(json::JsonValue::createObject());
        eventConfig.getObject()->set("enableLogging", json::JsonValue(true));
        eventConfig.getObject()->set("enableComponent", json::JsonValue(true));
        eventConfig.getObject()->set("enableStatistics", json::JsonValue(true));
        eventConfig.getObject()->set("asyncProcessing", json::JsonValue(false));
        eventConfig.getObject()->set("eventQueueSize", json::JsonValue(1000));
        eventConfig.getObject()->set("processingThreads", json::JsonValue(1));
        config.getObject()->set("event", eventConfig);

        // Logging settings
        json::JsonValue loggingConfig(json::JsonValue::createObject());
        loggingConfig.getObject()->set("consoleOutput", json::JsonValue(true));
        loggingConfig.getObject()->set("fileOutput", json::JsonValue(true));
        loggingConfig.getObject()->set("includeTimestamp", json::JsonValue(true));
        loggingConfig.getObject()->set("includeSeverity", json::JsonValue(true));
        loggingConfig.getObject()->set("includeSource", json::JsonValue(true));
        config.getObject()->set("logging", loggingConfig);

        // Convert to JSON string
        std::string jsonStr = json::Json::stringify(config, true); // Pretty print

        // Create the directory if it doesn't exist
        fs::path path(configFilePath);
        fs::path dir = path.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            try {
                fs::create_directories(dir);
            } catch (const std::exception& e) {
                unified_event::logError("ConfigurationManager", "Failed to create directory: " + dir.string() + " - " + e.what());
                return false;
            }
        }

        // Write to the file
        std::ofstream file(configFilePath);
        if (!file.is_open()) {
            unified_event::logError("ConfigurationManager", "Failed to open configuration file for writing: " + configFilePath);
            return false;
        }

        // Add a header comment
        file << "// DHT Hunter Configuration File\n";
        file << "// This file contains all configurable settings for the DHT Hunter application.\n";
        file << "// Modify these settings to customize the behavior of the application.\n\n";

        file << jsonStr;
        file.close();

        unified_event::logInfo("ConfigurationManager", "Successfully generated default configuration at " + configFilePath);
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("ConfigurationManager", "Exception while generating default configuration: " + std::string(e.what()));
        return false;
    }
}

std::string ConfigurationManager::getString(const std::string& key, const std::string& defaultValue) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, &defaultValue]() -> std::string {
            unified_event::logDebug("ConfigurationManager", "Getting string value for key: " + key);
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                unified_event::logDebug("ConfigurationManager", "No value found for key: " + key + ", using default: " + defaultValue);
                return defaultValue;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isString()) {
                    unified_event::logDebug("ConfigurationManager", "Value for key: " + key + " is not a string, using default: " + defaultValue);
                    return defaultValue;
                }
                std::string result = jsonValue.getString();
                unified_event::logDebug("ConfigurationManager", "Found string value for key: " + key + ", value: " + result);
                return result;
            } catch (const std::bad_any_cast&) {
                unified_event::logDebug("ConfigurationManager", "Bad any cast for key: " + key + ", using default: " + defaultValue);
                return defaultValue;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return defaultValue;
    }
}

int ConfigurationManager::getInt(const std::string& key, int defaultValue) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, defaultValue]() -> int {
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return defaultValue;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isNumber()) {
                    return defaultValue;
                }
                return jsonValue.getInt();
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return defaultValue;
    }
}

bool ConfigurationManager::getBool(const std::string& key, bool defaultValue) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, defaultValue]() -> bool {
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return defaultValue;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isBoolean()) {
                    return defaultValue;
                }
                return jsonValue.getBoolean();
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return defaultValue;
    }
}

double ConfigurationManager::getDouble(const std::string& key, double defaultValue) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, defaultValue]() -> double {
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return defaultValue;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isNumber()) {
                    return defaultValue;
                }
                return jsonValue.getNumber();
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return defaultValue;
    }
}

std::vector<std::string> ConfigurationManager::getStringArray(const std::string& key) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key]() -> std::vector<std::string> {
            std::vector<std::string> result;
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return result;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isArray()) {
                    return result;
                }

                {
                    const auto& array = jsonValue.getArray();
                    for (size_t i = 0; i < array->size(); ++i) {
                        auto item = array->get(i);
                        if (item.isString()) {
                            result.push_back(item.getString());
                        }
                    }
                }
                return result;
            } catch (const std::bad_any_cast&) {
                return result;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return std::vector<std::string>();
    }
}

std::vector<int> ConfigurationManager::getIntArray(const std::string& key) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key]() {
            std::vector<int> result;
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return result;
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                if (!jsonValue.isArray()) {
                    return result;
                }

                const auto& array = jsonValue.getArray();
                for (size_t i = 0; i < array->size(); ++i) {
                    auto item = array->get(i);
                    if (item.isNumber()) {
                        result.push_back(item.getInt());
                    }
                }
                return result;
            } catch (const std::bad_any_cast&) {
                return result;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return std::vector<int>();
    }
}

void ConfigurationManager::setString(const std::string& key, const std::string& value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, &value]() {
            auto keyPath = splitKeyPath(key);
            setValueAtPath(keyPath, std::make_shared<json::JsonValue>(value));
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::setInt(const std::string& key, int value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, value]() {
            auto keyPath = splitKeyPath(key);
            setValueAtPath(keyPath, std::make_shared<json::JsonValue>(value));
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::setBool(const std::string& key, bool value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, value]() {
            auto keyPath = splitKeyPath(key);
            setValueAtPath(keyPath, std::make_shared<json::JsonValue>(value));
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::setDouble(const std::string& key, double value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, value]() {
            auto keyPath = splitKeyPath(key);
            setValueAtPath(keyPath, std::make_shared<json::JsonValue>(value));
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::setStringArray(const std::string& key, const std::vector<std::string>& value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, &value]() {
            auto keyPath = splitKeyPath(key);
            json::JsonValue array(json::JsonValue::createArray());
            auto& arrayObj = array.getArray();
            for (const auto& item : value) {
                arrayObj->add(json::JsonValue(item));
            }
            setValueAtPath(keyPath, array);
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::setIntArray(const std::string& key, const std::vector<int>& value) {
    try {
        utility::thread::withLock(m_configMutex, [this, &key, &value]() {
            auto keyPath = splitKeyPath(key);
            json::JsonValue array(json::JsonValue::createArray());
            auto& arrayObj = array.getArray();
            for (const auto& item : value) {
                arrayObj->add(json::JsonValue(item));
            }
            setValueAtPath(keyPath, array);
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

bool ConfigurationManager::hasKey(const std::string& key) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key]() {
            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            return value.has_value();
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    }
}

std::string ConfigurationManager::getConfigFilePath() const {
    try {
        return utility::thread::withLock(m_configMutex, [this]() {
            return m_configFilePath;
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return "";
    }
}

bool ConfigurationManager::validateConfiguration() const {
    try {
        return utility::thread::withLock(m_configMutex, [this]() {
            if (!m_configLoaded) {
                unified_event::logError("ConfigurationManager", "No configuration loaded");
                return false;
            }

            // TODO: Implement validation rules for each configuration parameter
            // For now, just check if the root is an object
            try {
                auto jsonValue = std::any_cast<std::shared_ptr<json::JsonValue>>(m_configRoot);
                return jsonValue->isObject();
            } catch (const std::bad_any_cast&) {
                return false;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    }
}

std::optional<std::any> ConfigurationManager::getValueFromPath(const std::vector<std::string>& keyPath) const {
    if (keyPath.empty()) {
        unified_event::logDebug("ConfigurationManager", "Empty key path");
        return std::nullopt;
    }

    try {
        auto jsonValue = std::any_cast<json::JsonValue>(m_configRoot);
        if (!jsonValue.isObject()) {
            unified_event::logDebug("ConfigurationManager", "Root is not an object");
            return std::nullopt;
        }

        json::JsonValue current = jsonValue;
        for (size_t i = 0; i < keyPath.size() - 1; ++i) {
            if (!current.isObject()) {
                unified_event::logDebug("ConfigurationManager", "Path element is not an object: " + keyPath[i]);
                return std::nullopt;
            }

            json::JsonValue next = current.getObject()->get(keyPath[i]);
            if (next.isNull()) {
                unified_event::logDebug("ConfigurationManager", "Path element not found: " + keyPath[i]);
                return std::nullopt;
            }

            current = next;
        }

        if (!current.isObject()) {
            unified_event::logDebug("ConfigurationManager", "Final path element is not an object: " + keyPath.back());
            return std::nullopt;
        }

        json::JsonValue value = current.getObject()->get(keyPath.back());
        if (value.isNull()) {
            unified_event::logDebug("ConfigurationManager", "Value not found for key: " + keyPath.back());
            return std::nullopt;
        }

        unified_event::logDebug("ConfigurationManager", "Found value for key: " + keyPath.back());
        return value;
    } catch (const std::bad_any_cast&) {
        unified_event::logDebug("ConfigurationManager", "Bad any cast");
        return std::nullopt;
    }
}

bool ConfigurationManager::setValueAtPath(const std::vector<std::string>& keyPath, const std::any& value) {
    if (keyPath.empty()) {
        return false;
    }

    try {
        auto jsonValue = std::any_cast<json::JsonValue>(m_configRoot);
        if (!jsonValue.isObject()) {
            return false;
        }

        json::JsonValue current = jsonValue;
        for (size_t i = 0; i < keyPath.size() - 1; ++i) {
            if (!current.isObject()) {
                return false;
            }

            json::JsonValue next = current.getObject()->get(keyPath[i]);
            if (next.isNull()) {
                // Create a new object if the key doesn't exist
                next = json::JsonValue(json::JsonValue::createObject());
                current.getObject()->set(keyPath[i], next);
            } else if (!next.isObject()) {
                // Replace with an object if the value is not an object
                next = json::JsonValue(json::JsonValue::createObject());
                current.getObject()->set(keyPath[i], next);
            }

            current = next;
        }

        if (!current.isObject()) {
            return false;
        }

        try {
            auto jsonValueToSet = std::any_cast<json::JsonValue>(value);
            current.getObject()->set(keyPath.back(), jsonValueToSet);
            return true;
        } catch (const std::bad_any_cast&) {
            return false;
        }
    } catch (const std::bad_any_cast&) {
        return false;
    }
}

std::vector<std::string> ConfigurationManager::splitKeyPath(const std::string& key) {
    std::vector<std::string> result;
    std::string::size_type start = 0;
    std::string::size_type end = key.find('.');

    while (end != std::string::npos) {
        result.push_back(key.substr(start, end - start));
        start = end + 1;
        end = key.find('.', start);
    }

    result.push_back(key.substr(start));
    return result;
}

bool ConfigurationManager::enableHotReloading(bool enabled, int checkIntervalMs) {
    try {
        // If we're already in the desired state, do nothing
        if (m_hotReloadingEnabled.load() == enabled) {
            return true;
        }

        if (enabled) {
            // Check if the configuration file exists
            if (m_configFilePath.empty() || !fs::exists(m_configFilePath)) {
                unified_event::logError("ConfigurationManager", "Cannot enable hot-reloading: Configuration file does not exist");
                return false;
            }

            // Set the interval
            m_fileWatcherIntervalMs = checkIntervalMs;

            // Start the file watcher thread
            m_fileWatcherRunning = true;
            m_hotReloadingEnabled = true;

            // Create the thread
            m_fileWatcherThread = std::thread(&ConfigurationManager::fileWatcherThread, this);

            unified_event::logInfo("ConfigurationManager", "Hot-reloading enabled with interval " + std::to_string(checkIntervalMs) + " ms");
            return true;
        } else {
            // Stop the file watcher thread
            if (m_fileWatcherRunning.load()) {
                m_fileWatcherRunning = false;

                // Notify the thread to wake up and exit
                m_fileWatcherCondition.notify_one();

                // Wait for the thread to exit
                if (m_fileWatcherThread.joinable()) {
                    m_fileWatcherThread.join();
                }
            }

            m_hotReloadingEnabled = false;
            unified_event::logInfo("ConfigurationManager", "Hot-reloading disabled");
            return true;
        }
    } catch (const std::exception& e) {
        unified_event::logError("ConfigurationManager", "Exception while enabling/disabling hot-reloading: " + std::string(e.what()));
        return false;
    }
}

int ConfigurationManager::registerChangeCallback(const ConfigChangeCallback& callback, const std::string& key) {
    try {
        return utility::thread::withLock(m_callbacksMutex, [this, &callback, &key]() {
            int callbackId = m_nextCallbackId++;
            m_changeCallbacks[callbackId] = {callback, key};
            unified_event::logDebug("ConfigurationManager", "Registered change callback with ID " + std::to_string(callbackId) + " for key '" + key + "'");
            return callbackId;
        }, "ConfigurationManager::m_callbacksMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return -1;
    }
}

bool ConfigurationManager::unregisterChangeCallback(int callbackId) {
    try {
        return utility::thread::withLock(m_callbacksMutex, [this, callbackId]() {
            auto it = m_changeCallbacks.find(callbackId);
            if (it != m_changeCallbacks.end()) {
                m_changeCallbacks.erase(it);
                unified_event::logDebug("ConfigurationManager", "Unregistered change callback with ID " + std::to_string(callbackId));
                return true;
            }
            unified_event::logWarning("ConfigurationManager", "Failed to unregister change callback with ID " + std::to_string(callbackId) + ": Not found");
            return false;
        }, "ConfigurationManager::m_callbacksMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    }
}

void ConfigurationManager::notifyChangeCallbacks(const std::string& key) {
    try {
        // Make a copy of the callbacks to avoid holding the lock while calling them
        std::vector<std::pair<int, CallbackInfo>> callbacks;
        utility::thread::withLock(m_callbacksMutex, [this, &callbacks]() {
            callbacks.reserve(m_changeCallbacks.size());
            for (const auto& pair : m_changeCallbacks) {
                callbacks.push_back(pair);
            }
        }, "ConfigurationManager::m_callbacksMutex");

        // Call the callbacks
        for (const auto& pair : callbacks) {
            const auto& callbackInfo = pair.second;

            // If the callback is for a specific key, only call it if the key matches
            if (callbackInfo.key.empty() || key.empty() || key == callbackInfo.key) {
                try {
                    callbackInfo.callback(key);
                } catch (const std::exception& e) {
                    unified_event::logError("ConfigurationManager", "Exception in change callback: " + std::string(e.what()));
                }
            }
        }
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
    }
}

void ConfigurationManager::fileWatcherThread() {
    unified_event::logInfo("ConfigurationManager", "File watcher thread started");

    while (m_fileWatcherRunning.load()) {
        try {
            // Wait for the specified interval or until we're notified to exit
            std::unique_lock<std::mutex> lock(m_fileWatcherMutex);
            m_fileWatcherCondition.wait_for(lock, std::chrono::milliseconds(m_fileWatcherIntervalMs), [this]() {
                return !m_fileWatcherRunning.load();
            });

            // If we're not running anymore, exit
            if (!m_fileWatcherRunning.load()) {
                break;
            }

            // Check if the file has been modified
            if (!m_configFilePath.empty() && fs::exists(m_configFilePath)) {
                auto lastModifiedTime = fs::last_write_time(m_configFilePath);

                if (lastModifiedTime != m_lastModifiedTime) {
                    unified_event::logInfo("ConfigurationManager", "Configuration file has been modified, reloading");

                    // Reload the configuration
                    if (reloadConfiguration()) {
                        m_lastModifiedTime = lastModifiedTime;
                    }
                }
            }
        } catch (const std::exception& e) {
            unified_event::logError("ConfigurationManager", "Exception in file watcher thread: " + std::string(e.what()));
        }
    }

    unified_event::logInfo("ConfigurationManager", "File watcher thread stopped");
}

std::string ConfigurationManager::getConfigAsJson(bool pretty) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, pretty]() {
            if (!m_configLoaded) {
                return std::string("{}");
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(m_configRoot);
                return json::Json::stringify(jsonValue, pretty);
            } catch (const std::bad_any_cast&) {
                return std::string("{}");
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return std::string("{}");
    }
}

std::string ConfigurationManager::getConfigValueAsJson(const std::string& key, bool pretty) const {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, pretty]() {
            if (!m_configLoaded) {
                return std::string("");
            }

            auto keyPath = splitKeyPath(key);
            auto value = getValueFromPath(keyPath);
            if (!value.has_value()) {
                return std::string("");
            }

            try {
                auto jsonValue = std::any_cast<json::JsonValue>(*value);
                return json::Json::stringify(jsonValue, pretty);
            } catch (const std::bad_any_cast&) {
                return std::string("");
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return std::string("");
    }
}

bool ConfigurationManager::setConfigValueFromJson(const std::string& key, const std::string& jsonStr) {
    try {
        return utility::thread::withLock(m_configMutex, [this, &key, &jsonStr]() {
            if (!m_configLoaded) {
                unified_event::logError("ConfigurationManager", "No configuration loaded");
                return false;
            }

            // Parse the JSON string
            auto jsonValue = json::JsonValue::parse(jsonStr);
            if (jsonValue.isNull()) {
                unified_event::logError("ConfigurationManager", "Failed to parse JSON string");
                return false;
            }

            // Set the value
            auto keyPath = splitKeyPath(key);
            if (!setValueAtPath(keyPath, jsonValue)) {
                unified_event::logError("ConfigurationManager", "Failed to set value for key: " + key);
                return false;
            }

            // Notify callbacks
            notifyChangeCallbacks(key);

            return true;
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    } catch (const std::exception& e) {
        unified_event::logError("ConfigurationManager", "Exception while setting configuration value: " + std::string(e.what()));
        return false;
    }
}

} // namespace dht_hunter::utility::config
