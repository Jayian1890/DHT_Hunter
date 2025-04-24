#include "dht_hunter/metadata/bt_handshake.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <cstring>
#include <random>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("BTHandshake");
}

BTHandshake::BTHandshake()
    : protocolLength(BT_PROTOCOL_NAME_LENGTH) {
    
    std::fill(reserved.begin(), reserved.end(), 0);
    std::fill(infoHash.begin(), infoHash.end(), 0);
    std::fill(peerId.begin(), peerId.end(), 0);
    
    std::copy(BT_PROTOCOL_NAME, 
              BT_PROTOCOL_NAME + BT_PROTOCOL_NAME_LENGTH,
              protocol.begin());
}

BTHandshake::BTHandshake(const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
                         const std::array<uint8_t, BT_PEER_ID_LENGTH>& peerId,
                         bool enableExtensions)
    : protocolLength(BT_PROTOCOL_NAME_LENGTH),
      infoHash(infoHash),
      peerId(peerId) {
    
    std::fill(reserved.begin(), reserved.end(), 0);
    std::copy(BT_PROTOCOL_NAME, 
              BT_PROTOCOL_NAME + BT_PROTOCOL_NAME_LENGTH,
              protocol.begin());
    
    if (enableExtensions) {
        setExtensionProtocol(true);
    }
}

bool BTHandshake::supportsExtensionProtocol() const {
    return (reserved[5] & BT_EXTENSION_PROTOCOL_FLAG) != 0;
}

void BTHandshake::setExtensionProtocol(bool enable) {
    if (enable) {
        reserved[5] |= BT_EXTENSION_PROTOCOL_FLAG;
    } else {
        reserved[5] &= ~BT_EXTENSION_PROTOCOL_FLAG;
    }
}

std::vector<uint8_t> BTHandshake::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(BT_HANDSHAKE_LENGTH);
    
    // Protocol length
    result.push_back(protocolLength);
    
    // Protocol name
    result.insert(result.end(), protocol.begin(), protocol.end());
    
    // Reserved bytes
    result.insert(result.end(), reserved.begin(), reserved.end());
    
    // Info hash
    result.insert(result.end(), infoHash.begin(), infoHash.end());
    
    // Peer ID
    result.insert(result.end(), peerId.begin(), peerId.end());
    
    return result;
}

bool BTHandshake::deserialize(const uint8_t* data, size_t length) {
    if (!data || length < BT_HANDSHAKE_LENGTH) {
        logger->error("Invalid handshake data: too short (expected {}, got {})", 
                     BT_HANDSHAKE_LENGTH, length);
        return false;
    }
    
    // Check protocol length
    if (data[0] != BT_PROTOCOL_NAME_LENGTH) {
        logger->error("Invalid protocol length: expected {}, got {}", 
                     BT_PROTOCOL_NAME_LENGTH, data[0]);
        return false;
    }
    
    // Check protocol name
    if (std::memcmp(data + 1, BT_PROTOCOL_NAME, BT_PROTOCOL_NAME_LENGTH) != 0) {
        logger->error("Invalid protocol name");
        return false;
    }
    
    // Copy protocol length
    protocolLength = data[0];
    
    // Copy protocol name
    std::memcpy(protocol.data(), data + 1, BT_PROTOCOL_NAME_LENGTH);
    
    // Copy reserved bytes
    std::memcpy(reserved.data(), data + 1 + BT_PROTOCOL_NAME_LENGTH, BT_RESERVED_BYTES_LENGTH);
    
    // Copy info hash
    std::memcpy(infoHash.data(), 
               data + 1 + BT_PROTOCOL_NAME_LENGTH + BT_RESERVED_BYTES_LENGTH, 
               BT_INFOHASH_LENGTH);
    
    // Copy peer ID
    std::memcpy(peerId.data(), 
               data + 1 + BT_PROTOCOL_NAME_LENGTH + BT_RESERVED_BYTES_LENGTH + BT_INFOHASH_LENGTH, 
               BT_PEER_ID_LENGTH);
    
    return true;
}

std::array<uint8_t, BT_PEER_ID_LENGTH> generatePeerId(const std::string& prefix) {
    std::array<uint8_t, BT_PEER_ID_LENGTH> peerId;
    
    // Fill with random bytes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < BT_PEER_ID_LENGTH; ++i) {
        peerId[i] = static_cast<uint8_t>(dis(gen));
    }
    
    // Apply prefix if provided
    if (!prefix.empty()) {
        size_t prefixLength = std::min(prefix.length(), BT_PEER_ID_LENGTH);
        std::memcpy(peerId.data(), prefix.data(), prefixLength);
    }
    
    return peerId;
}

} // namespace dht_hunter::metadata
