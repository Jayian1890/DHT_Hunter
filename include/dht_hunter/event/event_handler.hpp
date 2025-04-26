#pragma once

#include "dht_hunter/event/event.hpp"
#include <memory>

namespace dht_hunter::event {

/**
 * @class EventHandler
 * @brief Base class for event handlers.
 */
class EventHandler {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~EventHandler() = default;

    /**
     * @brief Handle an event.
     * @param event The event to handle.
     */
    virtual void handle(const std::shared_ptr<Event>& event) = 0;
};

} // namespace dht_hunter::event
