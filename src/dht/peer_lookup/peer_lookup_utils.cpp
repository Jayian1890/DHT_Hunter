#include "dht_hunter/dht/peer_lookup/peer_lookup_utils.hpp"
#include <algorithm>

namespace dht_hunter::dht::peer_lookup_utils {

std::vector<std::shared_ptr<Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& targetID) {

    std::vector<std::shared_ptr<Node>> sortedNodes = nodes;

    std::sort(sortedNodes.begin(), sortedNodes.end(),
        [&targetID](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID distanceA = a->getID().distanceTo(targetID);
            NodeID distanceB = b->getID().distanceTo(targetID);
            return distanceA < distanceB;
        });

    return sortedNodes;
}

std::shared_ptr<Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const EndPoint& endpoint) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&endpoint](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == endpoint;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::shared_ptr<Node> findNodeByID(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& nodeID) {

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&nodeID](const std::shared_ptr<Node>& node) {
            return node->getID() == nodeID;
        });

    return (it != nodes.end()) ? *it : nullptr;
}

std::string generateLookupID(const InfoHash& infoHash) {
    return infoHashToString(infoHash);
}

} // namespace dht_hunter::dht::peer_lookup_utils
