# DHT Hunter Configuration System - Developer Guide

This document provides detailed information about the DHT Hunter configuration system architecture, how to use it in your code, and how to extend it with new configuration options.

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [Using the Configuration System](#using-the-configuration-system)
  - [Getting Configuration Values](#getting-configuration-values)
  - [Setting Configuration Values](#setting-configuration-values)
  - [Checking if a Key Exists](#checking-if-a-key-exists)
  - [Reloading Configuration](#reloading-configuration)
  - [Saving Configuration](#saving-configuration)
- [Adding New Configuration Options](#adding-new-configuration-options)
- [Validation Rules](#validation-rules)
- [Error Handling](#error-handling)
- [Best Practices](#best-practices)

## Architecture Overview

The configuration system is built around the `ConfigurationManager` class, which is implemented as a singleton. This class is responsible for loading, parsing, and providing access to configuration settings from a JSON file.

The system is designed to be user-friendly, automatically generating a default configuration file if one doesn't exist at the specified location. This ensures that the application always has a valid configuration to work with.

Key components:
- **ConfigurationManager**: Singleton class that manages configuration settings
- **JSON Parser**: Uses the `JsonValue` class to parse and manipulate JSON data
- **Configuration File**: JSON file containing all application settings
- **Command-Line Integration**: Allows overriding configuration settings via command-line arguments

The configuration system uses a dot notation to access nested configuration values. For example, `dht.port` refers to the `port` property in the `dht` object.

## Using the Configuration System

### Getting Configuration Values

To use the configuration system in your code, first get the singleton instance of the `ConfigurationManager`:

```cpp
#include "dht_hunter/utility/config/configuration_manager.hpp"

auto configManager = dht_hunter::utility::config::ConfigurationManager::getInstance();
```

Then, you can get configuration values using the appropriate getter methods:

```cpp
// Get a string value with a default
std::string logFile = configManager->getString("general.logFile", "dht_hunter.log");

// Get an integer value with a default
int dhtPort = configManager->getInt("dht.port", 6881);

// Get a boolean value with a default
bool enableLogging = configManager->getBool("event.enableLogging", true);

// Get a double value with a default
double someValue = configManager->getDouble("some.double.value", 1.0);

// Get a string array
std::vector<std::string> bootstrapNodes = configManager->getStringArray("dht.bootstrapNodes");

// Get an integer array
std::vector<int> someIntArray = configManager->getIntArray("some.int.array");
```

### Setting Configuration Values

You can also set configuration values programmatically:

```cpp
// Set a string value
configManager->setString("general.logFile", "new_log_file.log");

// Set an integer value
configManager->setInt("dht.port", 6882);

// Set a boolean value
configManager->setBool("event.enableLogging", false);

// Set a double value
configManager->setDouble("some.double.value", 2.0);

// Set a string array
std::vector<std::string> newBootstrapNodes = {"node1:6881", "node2:6881"};
configManager->setStringArray("dht.bootstrapNodes", newBootstrapNodes);

// Set an integer array
std::vector<int> newIntArray = {1, 2, 3};
configManager->setIntArray("some.int.array", newIntArray);
```

### Checking if a Key Exists

You can check if a configuration key exists:

```cpp
if (configManager->hasKey("dht.port")) {
    // The key exists
}
```

### Reloading Configuration

You can reload the configuration from the current file:

```cpp
if (configManager->reloadConfiguration()) {
    // Configuration reloaded successfully
} else {
    // Failed to reload configuration
}
```

### Saving Configuration

You can save the current configuration to a file:

```cpp
// Save to the current file
if (configManager->saveConfiguration()) {
    // Configuration saved successfully
} else {
    // Failed to save configuration
}

// Save to a specific file
if (configManager->saveConfiguration("/path/to/config.json")) {
    // Configuration saved successfully
} else {
    // Failed to save configuration
}
```

## Adding New Configuration Options

To add new configuration options:

1. Add the new option to the default configuration in `ConfigurationManager::generateDefaultConfiguration()`
2. Update the user documentation in `docs/Configuration_System.md`
3. Use the new option in your code as shown above

Example of adding a new option to the default configuration:

```cpp
auto newCategory = json::JsonValue::createObject();
newCategory->set("newOption", json::JsonValue("default value"));
config->set("newCategory", newCategory);
```

## Validation Rules

The configuration system performs basic validation when loading a configuration file:

1. The file must be valid JSON
2. The root must be a JSON object

Additional validation rules can be implemented in the `validateConfiguration()` method of the `ConfigurationManager` class. This method is called after loading a configuration file and can be used to validate specific configuration values.

Example of adding validation rules:

```cpp
bool ConfigurationManager::validateConfiguration() const {
    try {
        return utility::thread::withLock(m_configMutex, [this]() {
            if (!m_configLoaded) {
                unified_event::logError("ConfigurationManager", "No configuration loaded");
                return false;
            }

            // Check if the root is an object
            try {
                auto jsonValue = std::any_cast<std::shared_ptr<json::JsonValue>>(m_configRoot);
                if (!jsonValue->isObject()) {
                    return false;
                }

                // Validate specific values
                if (hasKey("dht.port")) {
                    int port = getInt("dht.port", 0);
                    if (port <= 0 || port > 65535) {
                        unified_event::logError("ConfigurationManager", "Invalid port number: " + std::to_string(port));
                        return false;
                    }
                }

                // Add more validation rules here

                return true;
            } catch (const std::bad_any_cast&) {
                return false;
            }
        }, "ConfigurationManager::m_configMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("ConfigurationManager", e.what());
        return false;
    }
}
```

## Error Handling

The configuration system uses the unified event system for logging errors. If a configuration value cannot be loaded or is invalid, an error message is logged and a default value is used.

When using the configuration system in your code, you should always provide a sensible default value when getting a configuration value. This ensures that your code will continue to work even if the configuration value is missing or invalid.

Example:

```cpp
// Provide a sensible default value
int dhtPort = configManager->getInt("dht.port", 6881);
```

## Best Practices

1. **Always provide default values**: When getting configuration values, always provide a sensible default value.
2. **Use dot notation**: Use dot notation to access nested configuration values.
3. **Document new options**: When adding new configuration options, update the user documentation.
4. **Validate configuration values**: Implement validation rules for configuration values to ensure they are valid.
5. **Handle errors gracefully**: Handle errors gracefully by providing sensible default values and logging errors.
6. **Use the configuration system consistently**: Use the configuration system consistently throughout the codebase.
7. **Group related options**: Group related configuration options together in the same category.
8. **Use descriptive names**: Use descriptive names for configuration options.
9. **Keep the configuration file simple**: Keep the configuration file simple and easy to understand.
10. **Provide examples**: Provide examples of how to use the configuration system in your code.
