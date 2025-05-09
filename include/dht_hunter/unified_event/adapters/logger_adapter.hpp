#pragma once

#include <string>
#include "../unified_event.hpp"

namespace dht_hunter {
namespace unified_event {
namespace adapters {

// Logger adapter class
class LoggerAdapter {
public:
    static void logInfo(const std::string& source, const std::string& message) {
        unified_event::logInfo(source, message);
    }
    
    static void logWarning(const std::string& source, const std::string& message) {
        unified_event::logWarning(source, message);
    }
    
    static void logError(const std::string& source, const std::string& message) {
        unified_event::logError(source, message);
    }
    
    static void logDebug(const std::string& source, const std::string& message) {
        unified_event::logDebug(source, message);
    }
};

} // namespace adapters
} // namespace unified_event
} // namespace dht_hunter
