#pragma once

#include "dht_hunter/dht/node_lookup/interfaces/node_lookup_component.hpp"
#include "utils/dht_core_utils.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <atomic>
#include <mutex>

namespace dht_hunter::dht::node_lookup {

/**
 * @brief Base class for node lookup components
 *
 * This class provides common functionality for all node lookup components.
 */
class BaseNodeLookupComponent : public NodeLookupComponent {
public:
    /**
     * @brief Constructs a base node lookup component
     * @param name The name of the component
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     */
    BaseNodeLookupComponent(const std::string& name,
                           const DHTConfig& config,
                           const NodeID& nodeID,
                           std::shared_ptr<RoutingTable> routingTable);

    /**
     * @brief Destructor
     */
    ~BaseNodeLookupComponent() override;

    /**
     * @brief Gets the name of the component
     * @return The name of the component
     */
    std::string getName() const override;

    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    bool start() override;

    /**
     * @brief Stops the component
     */
    void stop() override;

    /**
     * @brief Checks if the component is running
     * @return True if the component is running, false otherwise
     */
    bool isRunning() const override;

protected:
    /**
     * @brief Initializes the component (to be implemented by derived classes)
     * @return True if the component was initialized successfully, false otherwise
     */
    virtual bool onInitialize();

    /**
     * @brief Starts the component (to be implemented by derived classes)
     * @return True if the component was started successfully, false otherwise
     */
    virtual bool onStart();

    /**
     * @brief Stops the component (to be implemented by derived classes)
     */
    virtual void onStop();

    std::string m_name;
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::dht::node_lookup
