#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Event for peer discovery
 */
class PeerDiscoveredEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     * @param peer The discovered peer
     */
    PeerDiscoveredEvent(const std::string& source, const dht_hunter::dht::InfoHash& infoHash, const dht_hunter::network::NetworkAddress& peer)
        : Event(EventType::PeerDiscovered, EventSeverity::Debug, source) {
        setProperty("infoHash", infoHash);
        setProperty("peer", peer);
    }

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    dht_hunter::dht::InfoHash getInfoHash() const {
        auto infoHash = getProperty<dht_hunter::dht::InfoHash>("infoHash");
        return infoHash ? *infoHash : dht_hunter::dht::createEmptyInfoHash();
    }

    /**
     * @brief Gets the discovered peer
     * @return The discovered peer
     */
    dht_hunter::network::NetworkAddress getPeer() const {
        auto peer = getProperty<dht_hunter::network::NetworkAddress>("peer");
        return peer ? *peer : dht_hunter::network::NetworkAddress();
    }
};

/**
 * @brief Event for peer announcement
 */
class PeerAnnouncedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     * @param peer The announced peer
     * @param port The announced port
     */
    PeerAnnouncedEvent(const std::string& source, const dht_hunter::dht::InfoHash& infoHash, const dht_hunter::network::NetworkAddress& peer, uint16_t port)
        : Event(EventType::PeerAnnounced, EventSeverity::Debug, source) {
        setProperty("infoHash", infoHash);
        setProperty("peer", peer);
        setProperty("port", port);
    }

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    dht_hunter::dht::InfoHash getInfoHash() const {
        auto infoHash = getProperty<dht_hunter::dht::InfoHash>("infoHash");
        return infoHash ? *infoHash : dht_hunter::dht::createEmptyInfoHash();
    }

    /**
     * @brief Gets the announced peer
     * @return The announced peer
     */
    dht_hunter::network::NetworkAddress getPeer() const {
        auto peer = getProperty<dht_hunter::network::NetworkAddress>("peer");
        return peer ? *peer : dht_hunter::network::NetworkAddress();
    }

    /**
     * @brief Gets the announced port
     * @return The announced port
     */
    uint16_t getPort() const {
        auto port = getProperty<uint16_t>("port");
        return port ? *port : 0;
    }
};

} // namespace dht_hunter::unified_event
