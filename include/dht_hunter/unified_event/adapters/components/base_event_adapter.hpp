#pragma once

#include "dht_hunter/unified_event/adapters/interfaces/event_adapter_interface.hpp"
#include <mutex>

namespace dht_hunter::unified_event {

/**
 * @brief Base class for event adapters
 * 
 * This class provides common functionality for all event adapters.
 */
class BaseEventAdapter : public EventAdapterInterface {
public:
    /**
     * @brief Constructs a base event adapter
     * @param id The adapter ID
     */
    explicit BaseEventAdapter(const std::string& id);

    /**
     * @brief Destructor
     */
    ~BaseEventAdapter() override;

    /**
     * @brief Gets the adapter ID
     * @return The adapter ID
     */
    std::string getId() const override;

    /**
     * @brief Initializes the adapter
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the adapter
     */
    void shutdown() override;

protected:
    /**
     * @brief Initializes the adapter (to be implemented by derived classes)
     * @return True if initialization was successful, false otherwise
     */
    virtual bool onInitialize();

    /**
     * @brief Shuts down the adapter (to be implemented by derived classes)
     */
    virtual void onShutdown();

    std::string m_id;
    mutable std::mutex m_mutex;
    bool m_initialized;
};

} // namespace dht_hunter::unified_event
