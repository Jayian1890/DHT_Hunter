#include "dht_hunter/dht/core/dht_routing_table.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>

DEFINE_COMPONENT_LOGGER("DHT", "RoutingTable")

namespace dht_hunter::dht {

KBucket::KBucket(const NodeID& minDistance, const NodeID& maxDistance, size_t kSize)
    : m_minDistance(minDistance), m_maxDistance(maxDistance), m_kSize(kSize) {
}

KBucket::KBucket(KBucket&& other) noexcept
    : m_minDistance(std::move(other.m_minDistance)),
      m_maxDistance(std::move(other.m_maxDistance)),
      m_kSize(other.m_kSize),
      m_nodes(std::move(other.m_nodes)) {
}

KBucket& KBucket::operator=(KBucket&& other) noexcept {
    if (this != &other) {
        m_minDistance = std::move(other.m_minDistance);
        m_maxDistance = std::move(other.m_maxDistance);
        m_kSize = other.m_kSize;
        m_nodes = std::move(other.m_nodes);
    }
    return *this;
}

bool KBucket::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    // Check if the node is in the range of this bucket
    if (!isInRange(node->getID())) {
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Check if the node is already in the bucket
    auto it = m_nodes.find(nodeIDToString(node->getID()));
    if (it != m_nodes.end()) {
        // Update the node
        it->second = node;
        return true;
    }

    // Check if the bucket is full
    if (m_nodes.size() >= m_kSize) {
        // TODO: Implement node replacement strategy
        return false;
    }

    // Add the node to the bucket
    m_nodes[nodeIDToString(node->getID())] = node;
    return true;
}

bool KBucket::removeNode(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_nodes.erase(nodeIDToString(nodeID)) > 0;
}

std::shared_ptr<Node> KBucket::findNode(const NodeID& nodeID) const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    auto it = m_nodes.find(nodeIDToString(nodeID));
    if (it != m_nodes.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Node>> KBucket::getNodes() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    std::vector<std::shared_ptr<Node>> nodes;
    nodes.reserve(m_nodes.size());
    for (const auto& [_, node] : m_nodes) {
        nodes.push_back(node);
    }
    return nodes;
}

size_t KBucket::getNodeCount() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_nodes.size();
}

bool KBucket::isInRange(const NodeID& nodeID) const {
    // Calculate the distance from the node ID to the min and max distances
    NodeID distance = calculateDistance(nodeID, m_minDistance);
    return distance >= m_minDistance && distance <= m_maxDistance;
}

RoutingTable::RoutingTable(const NodeID& ownID, size_t kBucketSize)
    : m_ownID(ownID), m_kBucketSize(kBucketSize) {
    // Create the k-buckets
    // For now, we'll just create a single bucket for all nodes
    NodeID minDistance;
    std::fill(minDistance.begin(), minDistance.end(), 0);
    NodeID maxDistance;
    std::fill(maxDistance.begin(), maxDistance.end(), 0xFF);
    m_kBuckets.emplace_back(minDistance, maxDistance, kBucketSize);
}

bool RoutingTable::addNode(const NodeID& nodeID, const network::EndPoint& endpoint) {
    // Create a node object
    auto node = std::make_shared<Node>(nodeID, endpoint);
    return addNode(node);
}

bool RoutingTable::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    // Don't add ourselves to the routing table
    if (node->getID() == m_ownID) {
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Find the appropriate k-bucket for the node
    for (auto& kBucket : m_kBuckets) {
        if (kBucket.isInRange(node->getID())) {
            return kBucket.addNode(node);
        }
    }

    return false;
}

bool RoutingTable::removeNode(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Find the appropriate k-bucket for the node
    for (auto& kBucket : m_kBuckets) {
        if (kBucket.isInRange(nodeID)) {
            return kBucket.removeNode(nodeID);
        }
    }

    return false;
}

std::shared_ptr<Node> RoutingTable::findNode(const NodeID& nodeID) const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Find the appropriate k-bucket for the node
    for (const auto& kBucket : m_kBuckets) {
        if (kBucket.isInRange(nodeID)) {
            return kBucket.findNode(nodeID);
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<Node>> RoutingTable::getClosestNodes(const NodeID& targetID, size_t count) const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Get all nodes from all k-buckets
    std::vector<std::shared_ptr<Node>> allNodes;
    for (const auto& kBucket : m_kBuckets) {
        auto nodes = kBucket.getNodes();
        allNodes.insert(allNodes.end(), nodes.begin(), nodes.end());
    }

    // Sort the nodes by distance to the target ID
    std::sort(allNodes.begin(), allNodes.end(), [&targetID](const auto& a, const auto& b) {
        return calculateDistance(a->getID(), targetID) < calculateDistance(b->getID(), targetID);
    });

    // Return the closest nodes
    if (allNodes.size() > count) {
        allNodes.resize(count);
    }

    return allNodes;
}

size_t RoutingTable::getNodeCount() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& kBucket : m_kBuckets) {
        count += kBucket.getNodeCount();
    }
    return count;
}

bool RoutingTable::saveToFile(const std::string& filePath) const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    // Create a bencode dictionary to store the routing table
    bencode::BencodeDict dict;

    // Add the own ID to the dictionary
    auto ownIDValue = std::make_shared<bencode::BencodeValue>();
    ownIDValue->setString(nodeIDToString(m_ownID));
    dict["own_id"] = ownIDValue;

    // Add the k-bucket size to the dictionary
    auto kBucketSizeValue = std::make_shared<bencode::BencodeValue>();
    kBucketSizeValue->setString(std::to_string(m_kBucketSize));
    dict["k_bucket_size"] = kBucketSizeValue;

    // Add the nodes to the dictionary
    bencode::BencodeList nodesList;
    for (const auto& kBucket : m_kBuckets) {
        auto nodes = kBucket.getNodes();
        for (const auto& node : nodes) {
            bencode::BencodeDict nodeDict;

            auto idValue = std::make_shared<bencode::BencodeValue>();
            idValue->setString(nodeIDToString(node->getID()));
            nodeDict["id"] = idValue;

            auto ipValue = std::make_shared<bencode::BencodeValue>();
            ipValue->setString(node->getEndpoint().getAddress().toString());
            nodeDict["ip"] = ipValue;

            auto portValue = std::make_shared<bencode::BencodeValue>();
            portValue->setString(std::to_string(node->getEndpoint().getPort()));
            nodeDict["port"] = portValue;

            auto nodeDictValue = std::make_shared<bencode::BencodeValue>();
            nodeDictValue->setDict(nodeDict);
            nodesList.push_back(nodeDictValue);
        }
    }

    auto nodesListValue = std::make_shared<bencode::BencodeValue>();
    nodesListValue->setList(nodesList);
    dict["nodes"] = nodesListValue;

    // Encode the dictionary
    std::string encoded = bencode::encode(dict);

    // Save the encoded dictionary to the file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open routing table file for writing: {}", filePath);
        return false;
    }

    file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    if (!file) {
        getLogger()->error("Failed to write routing table to file: {}", filePath);
        return false;
    }

    getLogger()->info("Routing table saved to {}", filePath);
    return true;
}

bool RoutingTable::loadFromFile(const std::string& filePath) {
    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        getLogger()->error("Routing table file does not exist: {}", filePath);
        return false;
    }

    // Read the file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open routing table file for reading: {}", filePath);
        return false;
    }

    // Get the file size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file contents
    std::string encoded(static_cast<size_t>(size), '\0');
    if (!file.read(&encoded[0], size)) {
        getLogger()->error("Failed to read routing table from file: {}", filePath);
        return false;
    }

    // Decode the bencode data
    bencode::BencodeParser parser;
    bencode::BencodeValue root;

    try {
        root = parser.parse(encoded);
    } catch (const std::exception& e) {
        getLogger()->error("Failed to parse routing table: {}", e.what());
        return false;
    }

    // Check if the root is a dictionary
    if (!root.isDictionary()) {
        getLogger()->error("Routing table file is not a dictionary");
        return false;
    }

    // Get the dictionary
    const auto& dict = root.getDictionary();

    // Get the own ID
    auto ownIDIt = dict.find("own_id");
    if (ownIDIt == dict.end() || !ownIDIt->second->isString()) {
        getLogger()->error("Routing table file does not contain a valid own ID");
        return false;
    }
    auto ownIDOpt = stringToNodeID(ownIDIt->second->getString());
    if (!ownIDOpt) {
        getLogger()->error("Failed to parse own ID from routing table file");
        return false;
    }

    // Check if the own ID matches
    if (ownIDOpt.value() != m_ownID) {
        getLogger()->warning("Routing table file contains a different own ID");
    }

    // Get the k-bucket size
    auto kBucketSizeIt = dict.find("k_bucket_size");
    if (kBucketSizeIt == dict.end() || !kBucketSizeIt->second->isString()) {
        getLogger()->error("Routing table file does not contain a valid k-bucket size");
        return false;
    }
    size_t kBucketSize;
    try {
        kBucketSize = std::stoul(kBucketSizeIt->second->getString());
    } catch (const std::exception& e) {
        getLogger()->error("Failed to parse k-bucket size from routing table file: {}", e.what());
        return false;
    }

    // Check if the k-bucket size matches
    if (kBucketSize != m_kBucketSize) {
        getLogger()->warning("Routing table file contains a different k-bucket size");
    }

    // Get the nodes
    auto nodesIt = dict.find("nodes");
    if (nodesIt == dict.end() || !nodesIt->second->isList()) {
        getLogger()->error("Routing table file does not contain a valid nodes list");
        return false;
    }
    const auto& nodesList = nodesIt->second->getList();

    // Clear the routing table
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_kBuckets.clear();

    // Create the k-buckets
    // For now, we'll just create a single bucket for all nodes
    NodeID minDistance;
    std::fill(minDistance.begin(), minDistance.end(), 0);
    NodeID maxDistance;
    std::fill(maxDistance.begin(), maxDistance.end(), 0xFF);
    m_kBuckets.emplace_back(minDistance, maxDistance, m_kBucketSize);

    // Add the nodes to the routing table
    size_t addedNodes = 0;
    for (const auto& nodeValue : nodesList) {
        if (!nodeValue->isDictionary()) {
            getLogger()->warning("Node in routing table file is not a dictionary");
            continue;
        }
        const auto& nodeDict = nodeValue->getDictionary();

        // Get the node ID
        auto idIt = nodeDict.find("id");
        if (idIt == nodeDict.end() || !idIt->second->isString()) {
            getLogger()->warning("Node in routing table file does not contain a valid ID");
            continue;
        }
        auto nodeIDOpt = stringToNodeID(idIt->second->getString());
        if (!nodeIDOpt) {
            getLogger()->warning("Failed to parse node ID from routing table file");
            continue;
        }

        // Get the node IP
        auto ipIt = nodeDict.find("ip");
        if (ipIt == nodeDict.end() || !ipIt->second->isString()) {
            getLogger()->warning("Node in routing table file does not contain a valid IP");
            continue;
        }
        const auto& ip = ipIt->second->getString();

        // Get the node port
        auto portIt = nodeDict.find("port");
        if (portIt == nodeDict.end() || !portIt->second->isString()) {
            getLogger()->warning("Node in routing table file does not contain a valid port");
            continue;
        }
        uint16_t port;
        try {
            port = static_cast<uint16_t>(std::stoul(portIt->second->getString()));
        } catch (const std::exception& e) {
            getLogger()->warning("Failed to parse node port from routing table file: {}", e.what());
            continue;
        }

        // Create the node
        network::NetworkAddress address(ip);
        if (!address.isValid()) {
            getLogger()->warning("Failed to parse node IP from routing table file: {}", ip);
            continue;
        }
        network::EndPoint endpoint(address, port);
        auto node = std::make_shared<Node>(nodeIDOpt.value(), endpoint);

        // Add the node to the routing table
        if (addNode(node)) {
            addedNodes++;
        }
    }

    getLogger()->info("Loaded {} nodes from routing table file", addedNodes);
    return true;
}

} // namespace dht_hunter::dht
