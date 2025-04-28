#include "dht_hunter/utility/node/node_utils.hpp"
#include <algorithm>

namespace dht_hunter::utility::node {

std::vector<std::shared_ptr<types::Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& targetID) {

    std::vector<std::shared_ptr<types::Node>> sortedNodes = nodes;

    std::sort(sortedNodes.begin(), sortedNodes.end(),
        [&targetID](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
            types::NodeID distanceA = a->getID().distanceTo(targetID);
            types::NodeID distanceB = b->getID().distanceTo(targetID);
            return distanceA < distanceB;
        });

    return sortedNodes;
}

std::shared_ptr<types::Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::EndPoint& endpoint) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&endpoint](const std::shared_ptr<types::Node>& node) {
            return node->getEndpoint() == endpoint;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::shared_ptr<types::Node> findNodeByID(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& nodeID) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&nodeID](const std::shared_ptr<types::Node>& node) {
            return node->getID() == nodeID;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::string generateNodeLookupID(const types::NodeID& nodeID) {
    return types::nodeIDToString(nodeID);
}

std::string generatePeerLookupID(const types::InfoHash& infoHash) {
    return types::infoHashToString(infoHash);
}

} // namespace dht_hunter::utility::node
