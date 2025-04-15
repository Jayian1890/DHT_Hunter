#include <iostream>
#include "dht_hunter/logforge/logforge.hpp"

namespace lf = dht_hunter::logforge;

int main() {
    std::cout << "Testing isAsyncLoggingEnabled function" << std::endl;
    
    // Check initial state
    std::cout << "Initial state: " << (lf::LogForge::isAsyncLoggingEnabled() ? "true" : "false") << std::endl;
    
    // Initialize with async=false
    lf::LogForge::init(lf::LogLevel::INFO, lf::LogLevel::DEBUG, "async_test_false.log", true, false);
    std::cout << "After init with async=false: " << (lf::LogForge::isAsyncLoggingEnabled() ? "true" : "false") << std::endl;
    
    // Initialize with async=true
    lf::LogForge::init(lf::LogLevel::INFO, lf::LogLevel::DEBUG, "async_test_true.log", true, true);
    std::cout << "After init with async=true: " << (lf::LogForge::isAsyncLoggingEnabled() ? "true" : "false") << std::endl;
    
    return 0;
}
