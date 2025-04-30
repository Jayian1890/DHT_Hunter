#pragma once

#include "dht_hunter/dht/types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace dht_hunter::dht::bootstrap {

/**
 * @brief Interface for bootstrap components
 * 
 * This interface defines the common functionality for all bootstrap components.
 */
class BootstrapComponent {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~BootstrapComponent() = default;

    /**
     * @brief Gets the name of the component
     * @return The name of the component
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    virtual bool start() = 0;

    /**
     * @brief Stops the component
     */
    virtual void stop() = 0;

    /**
     * @brief Checks if the component is running
     * @return True if the component is running, false otherwise
     */
    virtual bool isRunning() const = 0;

    /**
     * @brief Bootstraps the DHT node
     * @param callback The callback to call when the bootstrap process is complete
     */
    virtual void bootstrap(std::function<void(bool)> callback) = 0;
};

} // namespace dht_hunter::dht::bootstrap
