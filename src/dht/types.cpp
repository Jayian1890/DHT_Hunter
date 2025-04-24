#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "Types")

namespace dht_hunter::dht {
std::string queryMethodToString(QueryMethod method) {
    switch (method) {
        case QueryMethod::Ping:
            return "ping";
        case QueryMethod::FindNode:
            return "find_node";
        case QueryMethod::GetPeers:
            return "get_peers";
        case QueryMethod::AnnouncePeer:
            return "announce_peer";
        default:
            return "unknown";
    }
}
QueryMethod stringToQueryMethod(const std::string& method) {
    if (method == "ping") {
        return QueryMethod::Ping;
    } else if (method == "find_node") {
        return QueryMethod::FindNode;
    } else if (method == "get_peers") {
        return QueryMethod::GetPeers;
    } else if (method == "announce_peer") {
        return QueryMethod::AnnouncePeer;
    } else {
        return QueryMethod::Unknown;
    }
}
NodeID generateRandomNodeID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);
    NodeID id;
    for (auto& byte : id) {
        byte = dist(gen);
    }
    return id;
}
TransactionID generateTransactionID(size_t length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);
    TransactionID id(length);
    for (auto& byte : id) {
        byte = dist(gen);
    }
    return id;
}
std::string nodeIDToString(const NodeID& id) {
    std::stringstream ss;
    for (auto byte : id) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}
std::optional<NodeID> stringToNodeID(const std::string& str) {
    if (str.length() != 40) {
        getLogger()->warning("Invalid node ID string length: {}", str.length());
        return std::nullopt;
    }
    NodeID id;
    for (size_t i = 0; i < 20; i++) {
        try {
            id[i] = static_cast<uint8_t>(std::stoi(str.substr(i * 2, 2), nullptr, 16));
        } catch (const std::exception& e) {
            getLogger()->warning("Invalid node ID string: {}", str);
            return std::nullopt;
        }
    }
    return id;
}
std::string transactionIDToString(const TransactionID& id) {
    std::stringstream ss;
    for (auto byte : id) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}
TransactionID stringToTransactionID(const std::string& str) {
    if (str.length() % 2 != 0) {
        getLogger()->warning("Invalid transaction ID string length: {}", str.length());
        return {};
    }
    TransactionID id(str.length() / 2);
    for (size_t i = 0; i < id.size(); i++) {
        try {
            id[i] = static_cast<uint8_t>(std::stoi(str.substr(i * 2, 2), nullptr, 16));
        } catch (const std::exception& e) {
            getLogger()->warning("Invalid transaction ID string: {}", str);
            return {};
        }
    }
    return id;
}
std::vector<CompactNodeInfo> parseCompactNodeInfo(const std::string& data) {
    std::vector<CompactNodeInfo> nodes;
    // Each node info is 26 bytes: 20 bytes node ID + 6 bytes endpoint (4 bytes IP + 2 bytes port)
    if (data.length() % 26 != 0) {
        getLogger()->warning("Invalid compact node info length: {}", data.length());
        return nodes;
    }
    for (size_t i = 0; i < data.length(); i += 26) {
        CompactNodeInfo node;
        // Extract node ID (20 bytes)
        std::memcpy(node.id.data(), data.data() + i, 20);
        // Extract IP address (4 bytes)
        uint32_t ipv4;
        std::memcpy(&ipv4, data.data() + i + 20, 4);
        // Extract port (2 bytes)
        uint16_t port;
        std::memcpy(&port, data.data() + i + 24, 2);
        port = ntohs(port);
        // Create endpoint
        node.endpoint = network::EndPoint(network::NetworkAddress(ntohl(ipv4)), port);
        nodes.push_back(node);
    }
    return nodes;
}
std::string encodeCompactNodeInfo(const std::vector<CompactNodeInfo>& nodes) {
    std::string result;
    result.reserve(nodes.size() * 26);
    for (const auto& node : nodes) {
        // Append node ID (20 bytes)
        result.append(reinterpret_cast<const char*>(node.id.data()), 20);
        // Append IP address (4 bytes)
        uint32_t ipv4 = htonl(node.endpoint.getAddress().getIPv4Address());
        result.append(reinterpret_cast<const char*>(&ipv4), 4);
        // Append port (2 bytes)
        uint16_t port = htons(node.endpoint.getPort());
        result.append(reinterpret_cast<const char*>(&port), 2);
    }
    return result;
}
} // namespace dht_hunter::dht
