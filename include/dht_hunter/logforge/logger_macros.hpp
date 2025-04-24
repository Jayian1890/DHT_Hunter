#pragma once

#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/log_initializer.hpp"

// Disable unused function warning for getLogger
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace dht_hunter::logforge {

/**
 * @brief Get a logger with the specified name.
 *
 * This function creates a logger with the specified name if it doesn't exist,
 * or returns an existing logger with that name.
 *
 * @param name The name of the logger.
 * @return A shared pointer to the logger.
 */
inline std::shared_ptr<LogForge> getLoggerByName(const std::string& name) {
    // Ensure logger system is initialized with default settings if not already done
    if (!LogInitializer::getInstance().isInitialized()) {
        LogInitializer::getInstance().initializeLogger();
    }
    return LogForge::getLogger(name);
}

} // namespace dht_hunter::logforge

/**
 * @brief Macro to define a logger for a file.
 *
 * This macro creates a function called getLogger() that returns a logger with the specified name.
 * The logger is created only once (lazy initialization) and then reused for subsequent calls.
 *
 * @param name The name of the logger.
 */
#define DEFINE_LOGGER(name) \
    namespace { \
        inline auto& getLogger() { \
            static auto logger = dht_hunter::logforge::getLoggerByName(name); \
            return logger; \
        } \
    }

/**
 * @brief Macro to define a logger for a file with a default name based on the component.
 *
 * This macro creates a function called getLogger() that returns a logger with a name
 * based on the component. The logger is created only once (lazy initialization) and
 * then reused for subsequent calls.
 *
 * @param component The component name (e.g., "DHT", "Network", "Bencode").
 * @param subcomponent The subcomponent name (e.g., "Node", "Socket", "Parser").
 */
#define DEFINE_COMPONENT_LOGGER(component, subcomponent) \
    DEFINE_LOGGER(component "." subcomponent)

// Re-enable unused function warning
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
