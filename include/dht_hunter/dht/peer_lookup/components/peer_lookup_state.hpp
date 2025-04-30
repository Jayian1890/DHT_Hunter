#pragma once

#include "dht_hunter/dht/types.hpp"
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>

namespace dht_hunter::dht::peer_lookup {

/**
 * @brief A lookup state for peer lookups
 */
class PeerLookupState {
public:
    /**
     * @brief Constructs a peer lookup state for a lookup operation
     * @param infoHash The info hash to look up
     * @param callback The callback to call with the result
     */
    PeerLookupState(const InfoHash& infoHash, std::function<void(const std::vector<EndPoint>&)> callback);

    /**
     * @brief Constructs a peer lookup state for an announce operation
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param callback The callback to call with the result
     */
    PeerLookupState(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback);

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const;

    /**
     * @brief Gets the peers
     * @return The peers
     */
    const std::vector<EndPoint>& getPeers() const;

    /**
     * @brief Gets the queried nodes
     * @return The queried nodes
     */
    const std::unordered_set<std::string>& getQueriedNodes() const;

    /**
     * @brief Gets the responded nodes
     * @return The responded nodes
     */
    const std::unordered_set<std::string>& getRespondedNodes() const;

    /**
     * @brief Gets the active queries
     * @return The active queries
     */
    const std::unordered_set<std::string>& getActiveQueries() const;

    /**
     * @brief Gets the node tokens
     * @return The node tokens
     */
    const std::unordered_map<std::string, std::string>& getNodeTokens() const;

    /**
     * @brief Gets the announced nodes
     * @return The announced nodes
     */
    const std::unordered_set<std::string>& getAnnouncedNodes() const;

    /**
     * @brief Gets the active announcements
     * @return The active announcements
     */
    const std::unordered_set<std::string>& getActiveAnnouncements() const;

    /**
     * @brief Gets the iteration
     * @return The iteration
     */
    size_t getIteration() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Gets the lookup callback
     * @return The lookup callback
     */
    const std::function<void(const std::vector<EndPoint>&)>& getLookupCallback() const;

    /**
     * @brief Gets the announce callback
     * @return The announce callback
     */
    const std::function<void(bool)>& getAnnounceCallback() const;

    /**
     * @brief Checks if the lookup is for an announce operation
     * @return True if the lookup is for an announce operation, false otherwise
     */
    bool isAnnouncing() const;

    /**
     * @brief Sets the nodes
     * @param nodes The nodes
     */
    void setNodes(const std::vector<std::shared_ptr<Node>>& nodes);

    /**
     * @brief Adds a node to the nodes
     * @param node The node to add
     */
    void addNode(std::shared_ptr<Node> node);

    /**
     * @brief Adds a peer to the peers
     * @param peer The peer to add
     */
    void addPeer(const EndPoint& peer);

    /**
     * @brief Adds a node to the queried nodes
     * @param nodeID The node ID to add
     */
    void addQueriedNode(const std::string& nodeID);

    /**
     * @brief Adds a node to the responded nodes
     * @param nodeID The node ID to add
     */
    void addRespondedNode(const std::string& nodeID);

    /**
     * @brief Adds a node to the active queries
     * @param nodeID The node ID to add
     */
    void addActiveQuery(const std::string& nodeID);

    /**
     * @brief Removes a node from the active queries
     * @param nodeID The node ID to remove
     */
    void removeActiveQuery(const std::string& nodeID);

    /**
     * @brief Adds a token for a node
     * @param nodeID The node ID
     * @param token The token
     */
    void addNodeToken(const std::string& nodeID, const std::string& token);

    /**
     * @brief Adds a node to the announced nodes
     * @param nodeID The node ID to add
     */
    void addAnnouncedNode(const std::string& nodeID);

    /**
     * @brief Adds a node to the active announcements
     * @param nodeID The node ID to add
     */
    void addActiveAnnouncement(const std::string& nodeID);

    /**
     * @brief Removes a node from the active announcements
     * @param nodeID The node ID to remove
     */
    void removeActiveAnnouncement(const std::string& nodeID);

    /**
     * @brief Increments the iteration
     */
    void incrementIteration();

    /**
     * @brief Checks if a node has been queried
     * @param nodeID The node ID to check
     * @return True if the node has been queried, false otherwise
     */
    bool hasBeenQueried(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has responded
     * @param nodeID The node ID to check
     * @return True if the node has responded, false otherwise
     */
    bool hasResponded(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has an active query
     * @param nodeID The node ID to check
     * @return True if the node has an active query, false otherwise
     */
    bool hasActiveQuery(const std::string& nodeID) const;

    /**
     * @brief Gets the token for a node
     * @param nodeID The node ID
     * @return The token, or an empty string if the node has no token
     */
    std::string getNodeToken(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has been announced
     * @param nodeID The node ID to check
     * @return True if the node has been announced, false otherwise
     */
    bool hasBeenAnnounced(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has an active announcement
     * @param nodeID The node ID to check
     * @return True if the node has an active announcement, false otherwise
     */
    bool hasActiveAnnouncement(const std::string& nodeID) const;

private:
    InfoHash m_infoHash;
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::vector<EndPoint> m_peers;
    std::unordered_set<std::string> m_queriedNodes;
    std::unordered_set<std::string> m_respondedNodes;
    std::unordered_set<std::string> m_activeQueries;
    std::unordered_map<std::string, std::string> m_nodeTokens;
    std::unordered_set<std::string> m_announcedNodes;
    std::unordered_set<std::string> m_activeAnnouncements;
    size_t m_iteration;
    uint16_t m_port;
    std::function<void(const std::vector<EndPoint>&)> m_lookupCallback;
    std::function<void(bool)> m_announceCallback;
    bool m_announcing;
};

} // namespace dht_hunter::dht::peer_lookup
