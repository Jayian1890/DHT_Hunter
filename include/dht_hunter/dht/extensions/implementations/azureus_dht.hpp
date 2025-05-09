#pragma once

#include "dht_hunter/dht/extensions/dht_extension.hpp"
#include <memory>

namespace dht_hunter::dht::extensions {

/**
 * @brief Implementation of the Azureus (Vuze) DHT protocol
 *
 * Azureus DHT is a DHT implementation used by the Vuze (formerly Azureus) BitTorrent client.
 * It has some differences from the Mainline DHT, including a different node ID format and
 * different message types.
 */
class AzureusDHT : public DHTExtension {
public:
    /**
     * @brief Constructs an Azureus DHT extension
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     */
    AzureusDHT(const DHTConfig& config,
              const NodeID& nodeID,
              std::shared_ptr<dht::RoutingTable> routingTable);

    /**
     * @brief Destructor
     */
    ~AzureusDHT() override;

    /**
     * @brief Gets the name of the extension
     * @return The name of the extension
     */
    std::string getName() const override;

    /**
     * @brief Gets the version of the extension
     * @return The version of the extension
     */
    std::string getVersion() const override;

    /**
     * @brief Initializes the extension
     * @return True if the extension was initialized successfully, false otherwise
     */
    bool initialize() override;

    /**
     * @brief Shuts down the extension
     */
    void shutdown() override;

    /**
     * @brief Checks if the extension is initialized
     * @return True if the extension is initialized, false otherwise
     */
    bool isInitialized() const override;

private:
    std::shared_ptr<dht::RoutingTable> m_routingTable;
    bool m_initialized;
};

} // namespace dht_hunter::dht::extensions
