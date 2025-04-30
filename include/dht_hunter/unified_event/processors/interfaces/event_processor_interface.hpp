#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include <memory>
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Interface for event processors
 * 
 * Event processors handle specific aspects of event processing,
 * such as logging, component communication, or statistics.
 */
class EventProcessorInterface {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~EventProcessorInterface() = default;
    
    /**
     * @brief Gets the processor ID
     * @return The processor ID
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief Determines if the processor should handle the event
     * @param event The event to check
     * @return True if the processor should handle the event, false otherwise
     */
    virtual bool shouldProcess(std::shared_ptr<Event> event) const = 0;
    
    /**
     * @brief Processes the event
     * @param event The event to process
     */
    virtual void process(std::shared_ptr<Event> event) = 0;
    
    /**
     * @brief Initializes the processor
     * @return True if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shuts down the processor
     */
    virtual void shutdown() = 0;
};

} // namespace dht_hunter::unified_event
