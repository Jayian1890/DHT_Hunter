#pragma once

#include "dht_hunter/dht/bootstrap/interfaces/bootstrap_component.hpp"
#include "utils/dht_core_utils.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <atomic>
#include <mutex>

namespace dht_hunter::dht::bootstrap {

using dht_hunter::utils::dht_core::DHTConfig;

/**
 * @brief Base class for bootstrap components
 *
 * This class provides common functionality for all bootstrap components.
 */
class BaseBootstrapComponent : public BootstrapComponent {
public:
    /**
     * @brief Constructs a base bootstrap component
     * @param name The name of the component
     * @param config The DHT configuration
     * @param nodeID The node ID
     */
    BaseBootstrapComponent(const std::string& name,
                          const DHTConfig& config,
                          const NodeID& nodeID);

    /**
     * @brief Destructor
     */
    ~BaseBootstrapComponent() override;

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

    /**
     * @brief Bootstraps the DHT node
     * @param callback The callback to call when the bootstrap process is complete
     */
    void bootstrap(std::function<void(bool)> callback) override;

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

    /**
     * @brief Bootstraps the DHT node (to be implemented by derived classes)
     * @param callback The callback to call when the bootstrap process is complete
     */
    virtual void onBootstrap(std::function<void(bool)> callback) = 0;

    std::string m_name;
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::dht::bootstrap
