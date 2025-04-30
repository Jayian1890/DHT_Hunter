#pragma once

#include "dht_hunter/unified_event/processors/interfaces/event_processor_interface.hpp"
#include <mutex>
#include <unordered_set>

namespace dht_hunter::unified_event {

/**
 * @brief Base class for event processors
 * 
 * This class provides common functionality for all event processors.
 */
class BaseEventProcessor : public EventProcessorInterface {
public:
    /**
     * @brief Constructs a base event processor
     * @param id The processor ID
     */
    explicit BaseEventProcessor(const std::string& id);

    /**
     * @brief Destructor
     */
    ~BaseEventProcessor() override;

    /**
     * @brief Gets the processor ID
     * @return The processor ID
     */
    std::string getId() const override;

    /**
     * @brief Determines if the processor should handle the event
     * @param event The event to check
     * @return True if the processor should handle the event, false otherwise
     */
    bool shouldProcess(std::shared_ptr<Event> event) const override;

    /**
     * @brief Processes the event
     * @param event The event to process
     */
    void process(std::shared_ptr<Event> event) override;

    /**
     * @brief Initializes the processor
     * @return True if initialization was successful, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the processor
     */
    void shutdown() override;

    /**
     * @brief Adds an event type to process
     * @param eventType The event type to add
     */
    void addEventType(EventType eventType);

    /**
     * @brief Removes an event type from processing
     * @param eventType The event type to remove
     */
    void removeEventType(EventType eventType);

    /**
     * @brief Adds an event severity to process
     * @param severity The event severity to add
     */
    void addEventSeverity(EventSeverity severity);

    /**
     * @brief Removes an event severity from processing
     * @param severity The event severity to remove
     */
    void removeEventSeverity(EventSeverity severity);

protected:
    /**
     * @brief Processes the event (to be implemented by derived classes)
     * @param event The event to process
     */
    virtual void onProcess(std::shared_ptr<Event> event) = 0;

    /**
     * @brief Initializes the processor (to be implemented by derived classes)
     * @return True if initialization was successful, false otherwise
     */
    virtual bool onInitialize();

    /**
     * @brief Shuts down the processor (to be implemented by derived classes)
     */
    virtual void onShutdown();

    std::string m_id;
    std::unordered_set<EventType> m_eventTypes;
    std::unordered_set<EventSeverity> m_eventSeverities;
    mutable std::mutex m_configMutex;
};

} // namespace dht_hunter::unified_event
