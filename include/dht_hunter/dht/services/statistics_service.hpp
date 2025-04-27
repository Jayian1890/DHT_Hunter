#pragma once

#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace dht_hunter::dht::services {

/**
 * @class StatisticsService
 * @brief Service for collecting and reporting DHT statistics
 *
 * This service collects statistics about the DHT network and provides
 * methods to retrieve them. It subscribes to DHT events to collect data.
 */
class StatisticsService {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<StatisticsService> getInstance();

    /**
     * @brief Destructor
     */
    ~StatisticsService();

    /**
     * @brief Starts the statistics service
     * @return True if the service was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the statistics service
     */
    void stop();

    /**
     * @brief Checks if the statistics service is running
     * @return True if the service is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the number of nodes discovered
     * @return The number of nodes discovered
     */
    size_t getNodesDiscovered() const;

    /**
     * @brief Gets the number of nodes added to the routing table
     * @return The number of nodes added to the routing table
     */
    size_t getNodesAdded() const;

    /**
     * @brief Gets the number of peers discovered
     * @return The number of peers discovered
     */
    size_t getPeersDiscovered() const;

    /**
     * @brief Gets the number of messages received
     * @return The number of messages received
     */
    size_t getMessagesReceived() const;

    /**
     * @brief Gets the number of messages sent
     * @return The number of messages sent
     */
    size_t getMessagesSent() const;

    /**
     * @brief Gets the number of errors
     * @return The number of errors
     */
    size_t getErrors() const;

    /**
     * @brief Gets statistics as a JSON string
     * @return The statistics as a JSON string
     */
    std::string getStatisticsAsJson() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    StatisticsService();

    /**
     * @brief Subscribes to events
     */
    void subscribeToEvents();

    /**
     * @brief Handles a node discovered event
     * @param event The event
     */
    void handleNodeDiscoveredEvent(std::shared_ptr<unified_event::Event> event);

    /**
     * @brief Handles a node added event
     * @param event The event
     */
    void handleNodeAddedEvent(std::shared_ptr<unified_event::Event> event);

    /**
     * @brief Handles a peer discovered event
     * @param event The event
     */
    void handlePeerDiscoveredEvent(std::shared_ptr<unified_event::Event> event);

    /**
     * @brief Handles a message received event
     * @param event The event
     */
    void handleMessageReceivedEvent(std::shared_ptr<unified_event::Event> event);

    /**
     * @brief Handles a message sent event
     * @param event The event
     */
    void handleMessageSentEvent(std::shared_ptr<unified_event::Event> event);

    /**
     * @brief Handles a system error event
     * @param event The event
     */
    void handleSystemErrorEvent(std::shared_ptr<unified_event::Event> event);

    // Singleton instance
    static std::shared_ptr<StatisticsService> s_instance;
    static std::mutex s_instanceMutex;

    // Event bus
    std::shared_ptr<unified_event::EventBus> m_eventBus;

    // Event subscription IDs
    std::vector<int> m_eventSubscriptionIds;

    // Statistics
    std::atomic<size_t> m_nodesDiscovered;
    std::atomic<size_t> m_nodesAdded;
    std::atomic<size_t> m_peersDiscovered;
    std::atomic<size_t> m_messagesReceived;
    std::atomic<size_t> m_messagesSent;
    std::atomic<size_t> m_errors;

    // Running state
    std::atomic<bool> m_running;

    // Logger    // Logger removed
};

} // namespace dht_hunter::dht::services
