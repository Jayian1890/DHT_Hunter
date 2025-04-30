#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>

namespace dht_hunter::unified_event {

/**
 * @brief Event for custom messages
 */
class CustomEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param message The custom message
     * @param severity The event severity
     */
    CustomEvent(const std::string& source, const std::string& message, EventSeverity severity = EventSeverity::Info)
        : Event(EventType::Custom, severity, source) {
        setProperty("message", message);
    }

    /**
     * @brief Gets the custom message
     * @return The custom message
     */
    std::string getMessage() const {
        auto message = getProperty<std::string>("message");
        return message ? *message : "";
    }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override {
        return getMessage();
    }
};

/**
 * @brief Event for when an InfoHash is discovered
 */
class InfoHashDiscoveredEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     */
    InfoHashDiscoveredEvent(const std::string& source, const types::InfoHash& infoHash)
        : Event(EventType::Custom, EventSeverity::Info, source) {
        setProperty("infoHash", types::infoHashToString(infoHash));
    }

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    types::InfoHash getInfoHash() const {
        auto infoHashStr = getProperty<std::string>("infoHash");
        if (!infoHashStr) {
            return types::InfoHash{};
        }

        types::InfoHash infoHash;
        if (!types::infoHashFromString(*infoHashStr, infoHash)) {
            return types::InfoHash{};
        }

        return infoHash;
    }

    /**
     * @brief Gets the event name
     * @return The event name
     */
    std::string getName() const override {
        return "InfoHashDiscovered";
    }
};

/**
 * @brief Event for when an InfoHash's metadata has been acquired
 */
class InfoHashMetadataAcquiredEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     * @param name The name of the torrent
     * @param size The total size of the torrent
     * @param fileCount The number of files in the torrent
     */
    InfoHashMetadataAcquiredEvent(const std::string& source,
                                 const types::InfoHash& infoHash,
                                 const std::string& name,
                                 uint64_t size,
                                 size_t fileCount)
        : Event(EventType::Custom, EventSeverity::Info, source) {
        setProperty("infoHash", types::infoHashToString(infoHash));
        setProperty("name", name);
        setProperty("size", std::to_string(size));
        setProperty("fileCount", std::to_string(fileCount));
    }

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    types::InfoHash getInfoHash() const {
        auto infoHashStr = getProperty<std::string>("infoHash");
        if (!infoHashStr) {
            return types::InfoHash{};
        }

        types::InfoHash infoHash;
        if (!types::infoHashFromString(*infoHashStr, infoHash)) {
            return types::InfoHash{};
        }

        return infoHash;
    }

    /**
     * @brief Gets the name of the torrent
     * @return The name
     */
    std::string getName() const override {
        auto name = getProperty<std::string>("name");
        return name ? *name : "InfoHashMetadataAcquired";
    }

    /**
     * @brief Gets the total size of the torrent
     * @return The size
     */
    uint64_t getSize() const {
        auto sizeStr = getProperty<std::string>("size");
        if (!sizeStr) {
            return 0;
        }

        try {
            return std::stoull(*sizeStr);
        } catch (const std::exception&) {
            return 0;
        }
    }

    /**
     * @brief Gets the number of files in the torrent
     * @return The file count
     */
    size_t getFileCount() const {
        auto fileCountStr = getProperty<std::string>("fileCount");
        if (!fileCountStr) {
            return 0;
        }

        try {
            return std::stoull(*fileCountStr);
        } catch (const std::exception&) {
            return 0;
        }
    }
};

} // namespace dht_hunter::unified_event
