#pragma once

#include "dht_hunter/dht/extensions/dht_extension.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include <memory>

namespace dht_hunter::dht::extensions {

/**
 * @brief Implementation of the Mainline DHT protocol
 * 
 * Mainline DHT is the most widely used DHT implementation in BitTorrent clients.
 * It is based on the Kademlia algorithm and is used for peer discovery.
 */
class MainlineDHT : public DHTExtension {
public:
    /**
     * @brief Constructs a Mainline DHT extension
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     */
    MainlineDHT(const DHTConfig& config, 
               const NodeID& nodeID,
               std::shared_ptr<RoutingTable> routingTable);

    /**
     * @brief Destructor
     */
    ~MainlineDHT() override;

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
    std::shared_ptr<RoutingTable> m_routingTable;
    bool m_initialized;
};

} // namespace dht_hunter::dht::extensions
