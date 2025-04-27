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

        // Store node details for logging
        if (node) {
            // Get the node ID and shorten it
            std::string nodeIDStr = dht_hunter::dht::nodeIDToString(node->getID());
            std::string shortNodeID = nodeIDStr.substr(0, 6);
            setProperty("nodeID", shortNodeID);

            // Get the endpoint
            setProperty("endpoint", node->getEndpoint().toString());

            // Calculate distance from our node if possible
            // Note: We don't have access to our node ID here, so this will be handled in the getName() method

            // Format last seen time
            auto lastSeen = node->getLastSeen();
            auto now = std::chrono::steady_clock::now();
            auto lastSeenSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastSeen).count();
            std::string lastSeenStr = (lastSeenSeconds == 0) ? "just now" : std::to_string(lastSeenSeconds) + " seconds ago";
            setProperty("lastSeen", lastSeenStr);
        }
    }

    /**
     * @brief Gets the discovered node
     * @return The discovered node
     */
    std::shared_ptr<dht_hunter::dht::Node> getNode() const {
        auto node = getProperty<std::shared_ptr<dht_hunter::dht::Node>>("node");
        return node ? *node : nullptr;
    }

    /**
     * @brief Gets detailed information about the node
     * @return A string with detailed information about the node
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto nodeID = getProperty<std::string>("nodeID");
        auto endpoint = getProperty<std::string>("endpoint");
        auto lastSeen = getProperty<std::string>("lastSeen");

        if (nodeID && endpoint && lastSeen) {
            ss << "ID: " << *nodeID;
            ss << ", endpoint: " << *endpoint;

            // Add bucket information if available
            auto bucket = getProperty<size_t>("bucket");
            if (bucket) {
                ss << ", bucket: " << *bucket;
            }

            ss << ", last seen: " << *lastSeen;

            // Add discovery method if available
            auto discoveryMethod = getProperty<std::string>("discoveryMethod");
            if (discoveryMethod) {
                ss << ", discovered " << *discoveryMethod;
            }
        }

        return ss.str();
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
        : Event(EventType::NodeAdded, EventSeverity::Info, source) {
        setProperty("node", node);
        setProperty("bucketIndex", bucketIndex);

        // Store node details for logging
        if (node) {
            // Get the node ID and shorten it
            std::string nodeIDStr = dht_hunter::dht::nodeIDToString(node->getID());
            std::string shortNodeID = nodeIDStr.substr(0, 6);
            setProperty("nodeID", shortNodeID);

            // Get the endpoint
            setProperty("endpoint", node->getEndpoint().toString());

            // Format last seen time
            auto lastSeen = node->getLastSeen();
            auto now = std::chrono::steady_clock::now();
            auto lastSeenSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastSeen).count();
            std::string lastSeenStr = (lastSeenSeconds == 0) ? "just now" : std::to_string(lastSeenSeconds) + " seconds ago";
            setProperty("lastSeen", lastSeenStr);
        }
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

    /**
     * @brief Gets detailed information about the node
     * @return A string with detailed information about the node
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto nodeID = getProperty<std::string>("nodeID");
        auto endpoint = getProperty<std::string>("endpoint");
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        auto lastSeen = getProperty<std::string>("lastSeen");

        if (nodeID && endpoint && lastSeen) {
            ss << "ID: " << *nodeID;
            ss << ", endpoint: " << *endpoint;

            if (bucketIndex) {
                ss << ", bucket: " << *bucketIndex;
            }

            ss << ", last seen: " << *lastSeen;
        }

        return ss.str();
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

        // Store node details for logging
        if (node) {
            // Get the node ID and shorten it
            std::string nodeIDStr = dht_hunter::dht::nodeIDToString(node->getID());
            std::string shortNodeID = nodeIDStr.substr(0, 6);
            setProperty("nodeID", shortNodeID);

            // Get the endpoint
            setProperty("endpoint", node->getEndpoint().toString());

            // Format last seen time
            auto lastSeen = node->getLastSeen();
            auto now = std::chrono::steady_clock::now();
            auto lastSeenSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastSeen).count();
            std::string lastSeenStr = (lastSeenSeconds == 0) ? "just now" : std::to_string(lastSeenSeconds) + " seconds ago";
            setProperty("lastSeen", lastSeenStr);
        }
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

    /**
     * @brief Gets detailed information about the node
     * @return A string with detailed information about the node
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto nodeID = getProperty<std::string>("nodeID");
        auto endpoint = getProperty<std::string>("endpoint");
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        auto lastSeen = getProperty<std::string>("lastSeen");

        if (nodeID && endpoint && lastSeen) {
            ss << "ID: " << *nodeID;
            ss << ", endpoint: " << *endpoint;

            if (bucketIndex) {
                ss << ", bucket: " << *bucketIndex;
            }

            ss << ", last seen: " << *lastSeen;

            // Add reason for removal if available
            auto reason = getProperty<std::string>("reason");
            if (reason) {
                ss << ", reason: " << *reason;
            }
        }

        return ss.str();
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

        // Store node details for logging
        if (node) {
            // Get the node ID and shorten it
            std::string nodeIDStr = dht_hunter::dht::nodeIDToString(node->getID());
            std::string shortNodeID = nodeIDStr.substr(0, 6);
            setProperty("nodeID", shortNodeID);

            // Get the endpoint
            setProperty("endpoint", node->getEndpoint().toString());

            // Format last seen time
            auto lastSeen = node->getLastSeen();
            auto now = std::chrono::steady_clock::now();
            auto lastSeenSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastSeen).count();
            std::string lastSeenStr = (lastSeenSeconds == 0) ? "just now" : std::to_string(lastSeenSeconds) + " seconds ago";
            setProperty("lastSeen", lastSeenStr);
        }
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

    /**
     * @brief Gets detailed information about the node
     * @return A string with detailed information about the node
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto nodeID = getProperty<std::string>("nodeID");
        auto endpoint = getProperty<std::string>("endpoint");
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        auto lastSeen = getProperty<std::string>("lastSeen");

        if (nodeID && endpoint && lastSeen) {
            ss << "ID: " << *nodeID;
            ss << ", endpoint: " << *endpoint;

            if (bucketIndex) {
                ss << ", bucket: " << *bucketIndex;
            }

            ss << ", last seen: " << *lastSeen;
        }

        return ss.str();
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

        // Store shortened target ID for logging
        if (!targetID.empty() && targetID.length() > 6) {
            std::string shortTargetID = targetID.substr(0, 6);
            setProperty("shortTargetID", shortTargetID);
        } else {
            setProperty("shortTargetID", targetID);
        }
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

    /**
     * @brief Gets detailed information about the bucket refresh
     * @return A string with detailed information about the bucket refresh
     */
    std::string getDetails() const override {
        std::stringstream ss;

        // Add detailed information if available
        auto bucketIndex = getProperty<size_t>("bucketIndex");
        auto shortTargetID = getProperty<std::string>("shortTargetID");

        if (bucketIndex && shortTargetID) {
            ss << "bucket: " << *bucketIndex;
            ss << ", target: " << *shortTargetID;

            // Add node count if available
            auto nodeCount = getProperty<size_t>("nodeCount");
            if (nodeCount) {
                ss << ", nodes: " << *nodeCount;
            }
        }

        return ss.str();
    }
};

} // namespace dht_hunter::unified_event
