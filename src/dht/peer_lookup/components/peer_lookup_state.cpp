#include "dht_hunter/dht/peer_lookup/components/peer_lookup_state.hpp"

namespace dht_hunter::dht::peer_lookup {

PeerLookupState::PeerLookupState(const InfoHash& infoHash, std::function<void(const std::vector<EndPoint>&)> callback)
    : m_infoHash(infoHash), m_iteration(0), m_lookupCallback(callback), m_announcing(false) {
}

PeerLookupState::PeerLookupState(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback)
    : m_infoHash(infoHash), m_iteration(0), m_port(port), m_announceCallback(callback), m_announcing(true) {
}

const InfoHash& PeerLookupState::getInfoHash() const {
    return m_infoHash;
}

const std::vector<std::shared_ptr<Node>>& PeerLookupState::getNodes() const {
    return m_nodes;
}

const std::vector<EndPoint>& PeerLookupState::getPeers() const {
    return m_peers;
}

const std::unordered_set<std::string>& PeerLookupState::getQueriedNodes() const {
    return m_queriedNodes;
}

const std::unordered_set<std::string>& PeerLookupState::getRespondedNodes() const {
    return m_respondedNodes;
}

const std::unordered_set<std::string>& PeerLookupState::getActiveQueries() const {
    return m_activeQueries;
}

const std::unordered_map<std::string, std::string>& PeerLookupState::getNodeTokens() const {
    return m_nodeTokens;
}

const std::unordered_set<std::string>& PeerLookupState::getAnnouncedNodes() const {
    return m_announcedNodes;
}

const std::unordered_set<std::string>& PeerLookupState::getActiveAnnouncements() const {
    return m_activeAnnouncements;
}

size_t PeerLookupState::getIteration() const {
    return m_iteration;
}

uint16_t PeerLookupState::getPort() const {
    return m_port;
}

const std::function<void(const std::vector<EndPoint>&)>& PeerLookupState::getLookupCallback() const {
    return m_lookupCallback;
}

const std::function<void(bool)>& PeerLookupState::getAnnounceCallback() const {
    return m_announceCallback;
}

bool PeerLookupState::isAnnouncing() const {
    return m_announcing;
}

void PeerLookupState::setNodes(const std::vector<std::shared_ptr<Node>>& nodes) {
    m_nodes = nodes;
}

void PeerLookupState::addNode(std::shared_ptr<Node> node) {
    m_nodes.push_back(node);
}

void PeerLookupState::addPeer(const EndPoint& peer) {
    // Check if the peer is already in the list
    for (const auto& existingPeer : m_peers) {
        if (existingPeer == peer) {
            return;
        }
    }

    m_peers.push_back(peer);
}

void PeerLookupState::addQueriedNode(const std::string& nodeID) {
    m_queriedNodes.insert(nodeID);
}

void PeerLookupState::addRespondedNode(const std::string& nodeID) {
    m_respondedNodes.insert(nodeID);
}

void PeerLookupState::addActiveQuery(const std::string& nodeID) {
    m_activeQueries.insert(nodeID);
}

void PeerLookupState::removeActiveQuery(const std::string& nodeID) {
    m_activeQueries.erase(nodeID);
}

void PeerLookupState::addNodeToken(const std::string& nodeID, const std::string& token) {
    m_nodeTokens[nodeID] = token;
}

void PeerLookupState::addAnnouncedNode(const std::string& nodeID) {
    m_announcedNodes.insert(nodeID);
}

void PeerLookupState::addActiveAnnouncement(const std::string& nodeID) {
    m_activeAnnouncements.insert(nodeID);
}

void PeerLookupState::removeActiveAnnouncement(const std::string& nodeID) {
    m_activeAnnouncements.erase(nodeID);
}

void PeerLookupState::incrementIteration() {
    m_iteration++;
}

bool PeerLookupState::hasBeenQueried(const std::string& nodeID) const {
    return m_queriedNodes.find(nodeID) != m_queriedNodes.end();
}

bool PeerLookupState::hasResponded(const std::string& nodeID) const {
    return m_respondedNodes.find(nodeID) != m_respondedNodes.end();
}

bool PeerLookupState::hasActiveQuery(const std::string& nodeID) const {
    return m_activeQueries.find(nodeID) != m_activeQueries.end();
}

std::string PeerLookupState::getNodeToken(const std::string& nodeID) const {
    auto it = m_nodeTokens.find(nodeID);
    if (it != m_nodeTokens.end()) {
        return it->second;
    }
    return "";
}

bool PeerLookupState::hasBeenAnnounced(const std::string& nodeID) const {
    return m_announcedNodes.find(nodeID) != m_announcedNodes.end();
}

bool PeerLookupState::hasActiveAnnouncement(const std::string& nodeID) const {
    return m_activeAnnouncements.find(nodeID) != m_activeAnnouncements.end();
}

} // namespace dht_hunter::dht::peer_lookup
