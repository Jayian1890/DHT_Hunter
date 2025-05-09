#include "utils/config_utils.hpp"
#include "utils/common_utils.hpp"
#include <fstream>
#include <sstream>
#include <chrono>

namespace dht_hunter::utility::config {

// Initialize static members
std::shared_ptr<ConfigurationManager> ConfigurationManager::s_instance = nullptr;
std::mutex ConfigurationManager::s_instanceMutex;

std::shared_ptr<ConfigurationManager> ConfigurationManager::getInstance(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<ConfigurationManager>(new ConfigurationManager(configFilePath));
    } else if (!configFilePath.empty() && configFilePath != s_instance->m_configFilePath) {
        s_instance->loadConfiguration(configFilePath);
    }
    return s_instance;
}

ConfigurationManager::ConfigurationManager(const std::string& configFilePath)
    : m_configRoot(json::Json::createObject()),
      m_configFilePath(configFilePath),
      m_configLoaded(false),
      m_hotReloadingEnabled(false),
      m_fileWatcherRunning(false),
      m_fileWatcherIntervalMs(1000),
      m_nextCallbackId(1) {
    
    if (!configFilePath.empty()) {
        loadConfiguration(configFilePath);
    }
}

ConfigurationManager::~ConfigurationManager() {
    // Stop the file watcher thread if it's running
    if (m_fileWatcherRunning) {
        m_fileWatcherRunning = false;
        m_fileWatcherCondition.notify_all();
        if (m_fileWatcherThread.joinable()) {
            m_fileWatcherThread.join();
        }
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    // Try to open the configuration file
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    // Read the file contents
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    try {
        // Parse the JSON
        auto jsonValue = json::Json::parse(buffer.str());
        if (!jsonValue.isObject()) {
            return false;
        }
        
        // Store the configuration
        m_configRoot = jsonValue.getObject();
        m_configFilePath = configFilePath;
        m_configLoaded = true;
        
        // Get the last modified time
        m_lastModifiedTime = std::filesystem::last_write_time(configFilePath);
        
        // Notify callbacks
        notifyChangeCallbacks();
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigurationManager::reloadConfiguration() {
    return loadConfiguration(m_configFilePath);
}

bool ConfigurationManager::saveConfiguration(const std::string& configFilePath) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    // Use the current file path if none is provided
    std::string filePath = configFilePath.empty() ? m_configFilePath : configFilePath;
    if (filePath.empty()) {
        return false;
    }
    
    try {
        // Convert the configuration to JSON
        auto jsonValue = json::JsonValue(std::any_cast<json::JsonValue::ObjectType>(m_configRoot));
        std::string jsonString = json::Json::stringify(jsonValue, true);
        
        // Write to the file
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        
        file << jsonString;
        file.close();
        
        // Update the file path and last modified time
        m_configFilePath = filePath;
        m_lastModifiedTime = std::filesystem::last_write_time(filePath);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigurationManager::generateDefaultConfiguration(const std::string& configFilePath) {
    // Create a default configuration
    auto configManager = std::shared_ptr<ConfigurationManager>(new ConfigurationManager(""));
    
    // Set default values
    configManager->setValue("dht.port", 6881);
    configManager->setValue("dht.bootstrap_nodes", std::vector<std::string>{
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881"
    });
    configManager->setValue("metadata.max_connections", 50);
    configManager->setValue("metadata.connection_timeout", 10000);
    configManager->setValue("web.enabled", true);
    configManager->setValue("web.port", 8080);
    configManager->setValue("logging.level", "info");
    
    // Save the configuration
    return configManager->saveConfiguration(configFilePath);
}

bool ConfigurationManager::hasKey(const std::string& key) const {
    auto keyPath = splitKeyPath(key);
    return getValueFromPath(keyPath).has_value();
}

std::vector<std::string> ConfigurationManager::getKeys() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    try {
        auto configRoot = std::any_cast<json::JsonValue::ObjectType>(m_configRoot);
        return configRoot->keys();
    } catch (const std::exception&) {
        return {};
    }
}

bool ConfigurationManager::enableHotReloading(bool enabled, int checkIntervalMs) {
    // If we're already in the desired state, do nothing
    if (m_hotReloadingEnabled == enabled) {
        return true;
    }
    
    m_hotReloadingEnabled = enabled;
    m_fileWatcherIntervalMs = checkIntervalMs;
    
    if (enabled) {
        // Start the file watcher thread
        m_fileWatcherRunning = true;
        m_fileWatcherThread = std::thread(&ConfigurationManager::fileWatcherThread, this);
    } else {
        // Stop the file watcher thread
        m_fileWatcherRunning = false;
        m_fileWatcherCondition.notify_all();
        if (m_fileWatcherThread.joinable()) {
            m_fileWatcherThread.join();
        }
    }
    
    return true;
}

int ConfigurationManager::registerChangeCallback(const ConfigChangeCallback& callback, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    
    int callbackId = m_nextCallbackId++;
    m_changeCallbacks[callbackId] = {callback, key};
    
    return callbackId;
}

bool ConfigurationManager::unregisterChangeCallback(int callbackId) {
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    
    return m_changeCallbacks.erase(callbackId) > 0;
}

void ConfigurationManager::fileWatcherThread() {
    while (m_fileWatcherRunning) {
        // Wait for the specified interval
        {
            std::unique_lock<std::mutex> lock(m_fileWatcherMutex);
            m_fileWatcherCondition.wait_for(lock, std::chrono::milliseconds(m_fileWatcherIntervalMs),
                [this] { return !m_fileWatcherRunning; });
        }
        
        // If we're shutting down, exit the thread
        if (!m_fileWatcherRunning) {
            break;
        }
        
        // Check if the file has been modified
        try {
            auto lastModifiedTime = std::filesystem::last_write_time(m_configFilePath);
            if (lastModifiedTime != m_lastModifiedTime) {
                // Reload the configuration
                reloadConfiguration();
            }
        } catch (const std::exception&) {
            // Ignore errors (file might not exist yet)
        }
    }
}

void ConfigurationManager::notifyChangeCallbacks(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    
    for (const auto& pair : m_changeCallbacks) {
        const auto& callbackInfo = pair.second;
        
        // If the callback is for a specific key and it doesn't match, skip it
        if (!callbackInfo.key.empty() && callbackInfo.key != key) {
            continue;
        }
        
        // Get the value for the callback
        std::any value;
        if (!key.empty()) {
            auto keyPath = splitKeyPath(key);
            auto result = getValueFromPath(keyPath);
            if (result.has_value()) {
                value = result.value();
            }
        }
        
        // Call the callback
        callbackInfo.callback(key, value);
    }
}

std::optional<std::any> ConfigurationManager::getValueFromPath(const std::vector<std::string>& keyPath) const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    if (!m_configLoaded || keyPath.empty()) {
        return std::nullopt;
    }
    
    try {
        auto configRoot = std::any_cast<json::JsonValue::ObjectType>(m_configRoot);
        json::JsonValue current(configRoot);
        
        for (size_t i = 0; i < keyPath.size(); ++i) {
            const auto& key = keyPath[i];
            
            if (!current.isObject()) {
                return std::nullopt;
            }
            
            auto obj = current.getObject();
            current = obj->get(key);
            
            if (current.isNull()) {
                return std::nullopt;
            }
            
            // If this is the last key, return the value
            if (i == keyPath.size() - 1) {
                if (current.isNull()) {
                    return std::nullopt;
                } else if (current.isBool()) {
                    return current.getBool();
                } else if (current.isNumber()) {
                    return current.getNumber();
                } else if (current.isString()) {
                    return current.getString();
                } else if (current.isObject()) {
                    return current.getObject();
                } else if (current.isArray()) {
                    return current.getArray();
                }
            }
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }
    
    return std::nullopt;
}

bool ConfigurationManager::setValueAtPath(const std::vector<std::string>& keyPath, const std::any& value) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    if (keyPath.empty()) {
        return false;
    }
    
    try {
        auto configRoot = std::any_cast<json::JsonValue::ObjectType>(m_configRoot);
        json::JsonValue current(configRoot);
        
        for (size_t i = 0; i < keyPath.size() - 1; ++i) {
            const auto& key = keyPath[i];
            
            if (!current.isObject()) {
                return false;
            }
            
            auto obj = current.getObject();
            auto next = obj->get(key);
            
            if (next.isNull()) {
                // Create a new object
                auto newObj = json::Json::createObject();
                obj->set(key, json::JsonValue(newObj));
                current = json::JsonValue(newObj);
            } else if (next.isObject()) {
                current = next;
            } else {
                return false;
            }
        }
        
        // Set the value at the final key
        const auto& lastKey = keyPath.back();
        
        if (!current.isObject()) {
            return false;
        }
        
        auto obj = current.getObject();
        
        // Convert the value to a JsonValue
        json::JsonValue jsonValue;
        
        try {
            if (value.type() == typeid(bool)) {
                jsonValue = json::JsonValue(std::any_cast<bool>(value));
            } else if (value.type() == typeid(int)) {
                jsonValue = json::JsonValue(static_cast<double>(std::any_cast<int>(value)));
            } else if (value.type() == typeid(double)) {
                jsonValue = json::JsonValue(std::any_cast<double>(value));
            } else if (value.type() == typeid(std::string)) {
                jsonValue = json::JsonValue(std::any_cast<std::string>(value));
            } else if (value.type() == typeid(json::JsonValue::ObjectType)) {
                jsonValue = json::JsonValue(std::any_cast<json::JsonValue::ObjectType>(value));
            } else if (value.type() == typeid(json::JsonValue::ArrayType)) {
                jsonValue = json::JsonValue(std::any_cast<json::JsonValue::ArrayType>(value));
            } else {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
        
        obj->set(lastKey, jsonValue);
        
        // Notify callbacks
        notifyChangeCallbacks(std::string(keyPath.begin(), keyPath.end()));
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> ConfigurationManager::splitKeyPath(const std::string& key) {
    std::vector<std::string> result;
    std::string current;
    
    for (char c : key) {
        if (c == '.') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}

} // namespace dht_hunter::utility::config
