#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/dht/types.hpp"
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

        // Store shortened info hash for logging
        std::string infoHashStr = dht_hunter::dht::infoHashToString(infoHash);
        std::string shortInfoHash = infoHashStr.substr(0, 6);
        setProperty("shortInfoHash", shortInfoHash);

        // Store peer address as string for logging
        setProperty("peerAddress", peer.toString());
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

    /**
     * @brief Gets detailed information about the peer
     * @return A string with detailed information about the peer
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto shortInfoHash = getProperty<std::string>("shortInfoHash");
        auto peerAddress = getProperty<std::string>("peerAddress");

        if (shortInfoHash && peerAddress) {
            ss << "Info Hash: " << *shortInfoHash;
            ss << ", endpoint: " << *peerAddress;

            // Add discovery source information
            std::string source = getSource();
            if (source.find("PeerStorage") != std::string::npos) {
                ss << ", source: storage";
            } else if (source.find("MessageHandler") != std::string::npos) {
                ss << ", source: incoming message";
            } else if (source.find("PeerLookup") != std::string::npos) {
                ss << ", source: DHT lookup";
            } else {
                ss << ", source: " << source;
            }
        }

        return ss.str();
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

    /**
     * @brief Gets detailed information about the peer announcement
     * @return A string with detailed information about the peer announcement
     */
    std::string getDetails() const override {
        std::stringstream ss;
        auto infoHash = getInfoHash();
        auto peer = getPeer();
        auto port = getPort();

        // Add info hash (shortened)
        std::string infoHashStr = dht_hunter::dht::infoHashToString(infoHash);
        std::string shortInfoHash = infoHashStr.substr(0, 6);
        ss << "Info Hash: " << shortInfoHash;

        // Add peer endpoint
        ss << ", endpoint: " << peer.toString();

        // Add port
        ss << ", port: " << port;

        return ss.str();
    }
};

} // namespace dht_hunter::unified_event
