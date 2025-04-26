#pragma once

#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <string>

namespace dht_hunter::dht::extensions {

/**
 * @brief Base class for DHT extensions
 */
class DHTExtension {
public:
    /**
     * @brief Constructs a DHT extension
     * @param config The DHT configuration
     * @param nodeID The node ID
     */
    DHTExtension(const DHTConfig& config, const NodeID& nodeID)
        : m_config(config), m_nodeID(nodeID), m_logger(event::Logger::forComponent("DHT.Extension")) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~DHTExtension() = default;

    /**
     * @brief Gets the name of the extension
     * @return The name of the extension
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Gets the version of the extension
     * @return The version of the extension
     */
    virtual std::string getVersion() const = 0;

    /**
     * @brief Initializes the extension
     * @return True if the extension was initialized successfully, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shuts down the extension
     */
    virtual void shutdown() = 0;

    /**
     * @brief Checks if the extension is initialized
     * @return True if the extension is initialized, false otherwise
     */
    virtual bool isInitialized() const = 0;

protected:
    DHTConfig m_config;
    NodeID m_nodeID;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht::extensions
