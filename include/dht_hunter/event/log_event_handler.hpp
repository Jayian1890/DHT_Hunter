#pragma once

#include "dht_hunter/event/event_handler.hpp"
#include "dht_hunter/event/log_event.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include <memory>

namespace dht_hunter::event {

/**
 * @class LogEventHandler
 * @brief Handler for log events that forwards them to LogForge.
 */
class LogEventHandler : public EventHandler, public std::enable_shared_from_this<LogEventHandler> {
public:
    /**
     * @brief Create a new LogEventHandler.
     * @return A shared pointer to the new handler.
     */
    static std::shared_ptr<LogEventHandler> create() {
        auto handler = std::shared_ptr<LogEventHandler>(new LogEventHandler());
        return handler;
    }

    /**
     * @brief Handle a log event.
     * @param event The event to handle.
     */
    void handle(const std::shared_ptr<Event>& event) override {
        // Check if this is a log event
        auto logEvent = std::dynamic_pointer_cast<LogEvent>(event);
        if (!logEvent) {
            return;
        }

        // Get the appropriate logger for the component
        auto logger = logforge::LogForge::getInstance().getLogger(logEvent->getComponent());

        // Log the message with the appropriate level
        switch (logEvent->getLevel()) {
            case logforge::LogLevel::TRACE:
                logger->trace(logEvent->getMessage());
                break;
            case logforge::LogLevel::DEBUG:
                logger->debug(logEvent->getMessage());
                break;
            case logforge::LogLevel::INFO:
                logger->info(logEvent->getMessage());
                break;
            case logforge::LogLevel::WARNING:
                logger->warning(logEvent->getMessage());
                break;
            case logforge::LogLevel::ERROR:
                logger->error(logEvent->getMessage());
                break;
            case logforge::LogLevel::CRITICAL:
                logger->critical(logEvent->getMessage());
                break;
        }
    }

private:
    /**
     * @brief Private constructor to enforce factory method.
     */
    LogEventHandler() = default;
};

} // namespace dht_hunter::event
