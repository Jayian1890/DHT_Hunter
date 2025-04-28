#pragma once

#include "dht_hunter/types/message_types.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <mutex>

namespace dht_hunter::bittorrent {

// Use the BTMessageID from the Types module
using BTMessageID = types::BTMessageID;

/**
 * @class BTMessageHandler
 * @brief Handles BitTorrent protocol messages related to DHT
 *
 * This class is responsible for handling BitTorrent protocol messages
 * that are related to the DHT, such as the PORT message which is used
 * for DHT node discovery.
 */
class BTMessageHandler {
public:
    /**
     * @brief Gets the singleton instance
     * @param routingManager The routing manager
     * @return The singleton instance
     */
    static std::shared_ptr<BTMessageHandler> getInstance(
        std::shared_ptr<dht::RoutingManager> routingManager = nullptr);

    /**
     * @brief Destructor
     */
    ~BTMessageHandler();

    /**
     * @brief Handles a PORT message
     * @param peerAddress The peer's address
     * @param data The message data
     * @param length The message length
     * @return True if the message was handled successfully, false otherwise
     */
    bool handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length);

    /**
     * @brief Handles a PORT message
     * @param peerAddress The peer's address
     * @param port The DHT port
     * @return True if the message was handled successfully, false otherwise
     */
    bool handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port);

    /**
     * @brief Creates a PORT message
     * @param port The DHT port
     * @return The PORT message
     */
    static std::vector<uint8_t> createPortMessage(uint16_t port);

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param routingManager The routing manager
     */
    explicit BTMessageHandler(std::shared_ptr<dht::RoutingManager> routingManager);

    // Singleton instance
    static std::shared_ptr<BTMessageHandler> s_instance;
    static std::mutex s_instanceMutex;

    // Routing manager
    std::shared_ptr<dht::RoutingManager> m_routingManager;

    // Logger    // Logger removed
};

} // namespace dht_hunter::bittorrent
