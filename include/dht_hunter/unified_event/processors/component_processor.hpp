#pragma once

#include "dht_hunter/unified_event/event_processor.hpp"
#include <string>
#include <unordered_set>
#include <mutex>

namespace dht_hunter::unified_event {

/**
 * @brief Configuration for the component processor
 */
struct ComponentProcessorConfig {
    // Set of event types to process
    std::unordered_set<EventType> eventTypes = {
        EventType::NodeDiscovered,
        EventType::NodeAdded,
        EventType::NodeRemoved,
        EventType::NodeUpdated,
        EventType::PeerDiscovered,
        EventType::PeerAnnounced,
        EventType::MessageSent,
        EventType::MessageReceived,
        EventType::SystemStarted,
        EventType::SystemStopped,
        EventType::SystemError
    };
};

/**
 * @brief Processor for component communication events
 */
class ComponentProcessor : public EventProcessor {
public:
    /**
     * @brief Constructor
     * @param config The component processor configuration
     */
    explicit ComponentProcessor(const ComponentProcessorConfig& config = ComponentProcessorConfig());
    
    /**
     * @brief Destructor
     */
    ~ComponentProcessor() override;
    
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
     * @brief Checks if an event type is being processed
     * @param eventType The event type to check
     * @return True if the event type is being processed, false otherwise
     */
    bool isProcessingEventType(EventType eventType) const;
    
private:
    ComponentProcessorConfig m_config;
    mutable std::mutex m_configMutex;
};

} // namespace dht_hunter::unified_event
