#include "dht_hunter/logging/logger.hpp"

namespace dht_hunter {
namespace logging {

// Initialize static members
std::mutex Logger::s_mutex;
std::vector<std::shared_ptr<LogSink>> Logger::s_sinks;
std::unordered_map<std::string, std::shared_ptr<Logger>> Logger::s_loggers;
LogLevel Logger::s_globalLevel = LogLevel::TRACE;
bool Logger::s_initialized = false;

// Add some implementation methods here if needed

} // namespace logging
} // namespace dht_hunter
