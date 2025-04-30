#include "dht_hunter/dht/node_lookup/components/node_lookup_state.hpp"

namespace dht_hunter::dht::node_lookup {

NodeLookupState::NodeLookupState(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback)
    : m_targetID(targetID), m_iteration(0), m_callback(callback) {
}

const NodeID& NodeLookupState::getTargetID() const {
    return m_targetID;
}

const std::vector<std::shared_ptr<Node>>& NodeLookupState::getNodes() const {
    return m_nodes;
}

const std::unordered_set<std::string>& NodeLookupState::getQueriedNodes() const {
    return m_queriedNodes;
}

const std::unordered_set<std::string>& NodeLookupState::getRespondedNodes() const {
    return m_respondedNodes;
}

const std::unordered_set<std::string>& NodeLookupState::getActiveQueries() const {
    return m_activeQueries;
}

size_t NodeLookupState::getIteration() const {
    return m_iteration;
}

const std::function<void(const std::vector<std::shared_ptr<Node>>&)>& NodeLookupState::getCallback() const {
    return m_callback;
}

void NodeLookupState::setNodes(const std::vector<std::shared_ptr<Node>>& nodes) {
    m_nodes = nodes;
}

void NodeLookupState::addNode(std::shared_ptr<Node> node) {
    m_nodes.push_back(node);
}

void NodeLookupState::addQueriedNode(const std::string& nodeID) {
    m_queriedNodes.insert(nodeID);
}

void NodeLookupState::addRespondedNode(const std::string& nodeID) {
    m_respondedNodes.insert(nodeID);
}

void NodeLookupState::addActiveQuery(const std::string& nodeID) {
    m_activeQueries.insert(nodeID);
}

void NodeLookupState::removeActiveQuery(const std::string& nodeID) {
    m_activeQueries.erase(nodeID);
}

void NodeLookupState::incrementIteration() {
    m_iteration++;
}

bool NodeLookupState::hasBeenQueried(const std::string& nodeID) const {
    return m_queriedNodes.find(nodeID) != m_queriedNodes.end();
}

bool NodeLookupState::hasResponded(const std::string& nodeID) const {
    return m_respondedNodes.find(nodeID) != m_respondedNodes.end();
}

bool NodeLookupState::hasActiveQuery(const std::string& nodeID) const {
    return m_activeQueries.find(nodeID) != m_activeQueries.end();
}

} // namespace dht_hunter::dht::node_lookup
