#include "dht_hunter/logforge/log_initializer.hpp"

namespace {
    // This static variable will be initialized during static initialization,
    // before main() is called. This ensures the logger is available for use
    // by any code that runs before main().
    const auto& g_globalLogInitializer = dht_hunter::logforge::LogInitializer::getInstance();
}

namespace dht_hunter::logforge {
    // This function is defined here to ensure it's linked into the executable
    // even if it's not explicitly called elsewhere.
    void ensureLoggerInitialized() {
        if (!LogInitializer::getInstance().isInitialized()) {
            LogInitializer::getInstance().initializeLogger();
        }
    }
}
