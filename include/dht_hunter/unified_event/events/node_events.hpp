#pragma once

#include "dht_hunter/unified_event/event.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include <string>
#include <memory>

namespace dht_hunter::unified_event {

/**
 * @brief Event for node discovery
 */
class NodeDiscoveredEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param node The discovered node
     */
    NodeDiscoveredEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Node> node)
        : Event(EventType::NodeDiscovered, EventSeverity::Debug, source) {
        setProperty("node", node);
    }

    /**
     * @brief Gets the discovered node
     * @return The discovered node
     */
    std::shared_ptr<dht_hunter::dht::Node> getNode() const {
        auto node = getProperty<std::shared_ptr<dht_hunter::dht::Node>>("node");
        return node ? *node : nullptr;
    }
};

/**
 * @brief Event for node addition to the routing table
 */
class NodeAddedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param node The added node
     * @param bucketIndex The bucket index where the node was added
     */
    NodeAddedEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Node> node, size_t bucketIndex)
        : Event(EventType::NodeAdded, EventSeverity::Debug, source) {
        setProperty("node", node);
        setProperty("bucketIndex", bucketIndex);
    }

    /**
     * @brief Gets the added node
     * @return The added node
     */
    std::shared_ptr<dht_hunter::dht::Node> getNode() const {
        auto node = getProperty<std::shared_ptr<dht_hunter::dht::Node>>("node");
        return node ? *node : nullptr;
    }

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    size_t getBucketIndex() const {
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        return bucketIndex ? *bucketIndex : 0;
    }
};

/**
 * @brief Event for node removal from the routing table
 */
class NodeRemovedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param node The removed node
     * @param bucketIndex The bucket index where the node was removed from
     */
    NodeRemovedEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Node> node, size_t bucketIndex)
        : Event(EventType::NodeRemoved, EventSeverity::Debug, source) {
        setProperty("node", node);
        setProperty("bucketIndex", bucketIndex);
    }

    /**
     * @brief Gets the removed node
     * @return The removed node
     */
    std::shared_ptr<dht_hunter::dht::Node> getNode() const {
        auto node = getProperty<std::shared_ptr<dht_hunter::dht::Node>>("node");
        return node ? *node : nullptr;
    }

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    size_t getBucketIndex() const {
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        return bucketIndex ? *bucketIndex : 0;
    }
};

/**
 * @brief Event for node update in the routing table
 */
class NodeUpdatedEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param source The source of the event
     * @param node The updated node
     * @param bucketIndex The bucket index where the node was updated
     */
    NodeUpdatedEvent(const std::string& source, std::shared_ptr<dht_hunter::dht::Node> node, size_t bucketIndex)
        : Event(EventType::NodeUpdated, EventSeverity::Debug, source) {
        setProperty("node", node);
        setProperty("bucketIndex", bucketIndex);
    }

    /**
     * @brief Gets the updated node
     * @return The updated node
     */
    std::shared_ptr<dht_hunter::dht::Node> getNode() const {
        auto node = getProperty<std::shared_ptr<dht_hunter::dht::Node>>("node");
        return node ? *node : nullptr;
    }

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    size_t getBucketIndex() const {
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        return bucketIndex ? *bucketIndex : 0;
    }
};

/**
 * @brief Event for bucket refresh
 */
class BucketRefreshEvent : public Event {
public:
    /**
     * @brief Constructor
     * @param bucketIndex The index of the bucket being refreshed
     * @param targetID The target ID for the refresh lookup
     */
    BucketRefreshEvent(size_t bucketIndex, const std::string& targetID)
        : Event(EventType::BucketRefreshed, EventSeverity::Debug, "DHT.RoutingTable") {
        setProperty("bucketIndex", bucketIndex);
        setProperty("targetID", targetID);
    }

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    size_t getBucketIndex() const {
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        return bucketIndex ? *bucketIndex : 0;
    }

    /**
     * @brief Gets the target ID
     * @return The target ID
     */
    std::string getTargetID() const {
        auto targetID = getProperty<std::string>("targetID");
        return targetID ? *targetID : "";
    }
};

} // namespace dht_hunter::unified_event
