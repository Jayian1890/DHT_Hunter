#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <memory>
#include <string>

namespace dht_hunter::bittorrent::events {

/**
 * @brief Event for metadata acquisition success
 */
class MetadataAcquiredEvent : public unified_event::Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     * @param metadata The metadata
     */
    MetadataAcquiredEvent(const std::string& source, const types::InfoHash& infoHash, 
                         std::shared_ptr<bencode::BencodeValue> metadata)
        : Event(unified_event::EventType::Custom, unified_event::EventSeverity::Info, source) {
        setProperty("infoHash", infoHash);
        setProperty("metadata", metadata);
    }
    
    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    types::InfoHash getInfoHash() const {
        auto infoHash = getProperty<types::InfoHash>("infoHash");
        return infoHash ? *infoHash : types::InfoHash();
    }
    
    /**
     * @brief Gets the metadata
     * @return The metadata
     */
    std::shared_ptr<bencode::BencodeValue> getMetadata() const {
        auto metadata = getProperty<std::shared_ptr<bencode::BencodeValue>>("metadata");
        return metadata ? *metadata : nullptr;
    }
};

/**
 * @brief Event for metadata acquisition failure
 */
class MetadataAcquisitionFailedEvent : public unified_event::Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param infoHash The info hash
     * @param reason The reason for the failure
     */
    MetadataAcquisitionFailedEvent(const std::string& source, const types::InfoHash& infoHash, 
                                  const std::string& reason)
        : Event(unified_event::EventType::Custom, unified_event::EventSeverity::Warning, source) {
        setProperty("infoHash", infoHash);
        setProperty("reason", reason);
    }
    
    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    types::InfoHash getInfoHash() const {
        auto infoHash = getProperty<types::InfoHash>("infoHash");
        return infoHash ? *infoHash : types::InfoHash();
    }
    
    /**
     * @brief Gets the reason for the failure
     * @return The reason
     */
    std::string getReason() const {
        auto reason = getProperty<std::string>("reason");
        return reason ? *reason : "";
    }
};

} // namespace dht_hunter::bittorrent::events
