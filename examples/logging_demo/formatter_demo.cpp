#include <iostream>
#include "dht_hunter/logging/formatter.hpp"
#include "dht_hunter/logging/logger.hpp"

namespace dl = dht_hunter::logging;

int main() {
    std::cout << "LogForge Formatter Demo" << std::endl;
    std::cout << "======================" << std::endl;

    // Initialize the logging system
    dl::Logger::init(dl::LogLevel::TRACE, dl::LogLevel::TRACE, "formatter_demo.log", true);

    // Get a logger
    auto logger = dl::Logger::getLogger("FormatterDemo");

    // Basic formatting
    logger->info("Basic formatting: Hello, {}!", "World");

    // Multiple placeholders
    logger->info("Multiple placeholders: {}, {} and {}", "Apple", "Banana", "Cherry");

    // Different types
    logger->info("Different types: String={}, Integer={}, Float={}, Bool={}",
                "test", 42, 3.14159, true);

    // Positional arguments
    logger->info("Positional arguments: {1}, {0}, {2}", "B", "A", "C");

    // Escaped braces
    logger->info("Escaped braces: {{not a placeholder}} but {} is", "this");

    // Custom object
    struct Point {
        int x, y;
    };

    // Define the stream operator
    auto pointPrinter = [](const Point& p) -> std::string {
        std::ostringstream oss;
        oss << "(" << p.x << ", " << p.y << ")";
        return oss.str();
    };

    Point p{10, 20};
    logger->info("Custom object: The point is at {}", pointPrinter(p));

    // Demonstrate different log levels with formatting
    logger->trace("Trace message with formatting: Value={}", 42);
    logger->debug("Debug message with formatting: Value={}", 3.14159);
    logger->warning("Warning message with formatting: Status={}", "Caution");
    logger->error("Error message with formatting: Code={}", 404);
    logger->critical("Critical message with formatting: System={}", "Failure");

    std::cout << "Formatted log messages have been written to the console and formatter_demo.log" << std::endl;

    return 0;
}
