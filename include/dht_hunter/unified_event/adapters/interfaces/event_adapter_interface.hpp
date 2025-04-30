#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include <memory>
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Interface for event adapters
 * 
 * Event adapters convert between different event systems.
 */
class EventAdapterInterface {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~EventAdapterInterface() = default;
    
    /**
     * @brief Gets the adapter ID
     * @return The adapter ID
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief Initializes the adapter
     * @return True if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Shuts down the adapter
     */
    virtual void shutdown() = 0;
};

} // namespace dht_hunter::unified_event
