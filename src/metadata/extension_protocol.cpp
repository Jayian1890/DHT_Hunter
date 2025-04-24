#include "dht_hunter/metadata/extension_protocol.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <cstring>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("ExtensionProtocol");
}

ExtensionProtocol::ExtensionProtocol()
    : m_metadataSize(0), m_peerPort(0) {
    
    // Register supported extensions
    m_localExtensions["ut_metadata"] = 1;
}

std::vector<uint8_t> ExtensionProtocol::createHandshake(uint16_t localPort, 
                                                       const std::string& clientVersion,
                                                       uint32_t metadataSize) const {
    // Create the handshake dictionary
    auto handshake = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Add extension message IDs
    auto extensions = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    for (const auto& [name, id] : m_localExtensions) {
        extensions->setInteger(name, id);
    }
    handshake->set("m", extensions);
    
    // Add optional fields
    if (localPort > 0) {
        handshake->setInteger("p", localPort);
    }
    
    if (!clientVersion.empty()) {
        handshake->setString("v", clientVersion);
    }
    
    if (metadataSize > 0) {
        handshake->setInteger("metadata_size", metadataSize);
    }
    
    // Encode the handshake
    std::string encodedHandshake = bencode::BencodeEncoder::encode(*handshake);
    
    // Create the extension message
    std::vector<uint8_t> message;
    message.reserve(encodedHandshake.size() + 2);
    
    // Extension message ID
    message.push_back(BT_EXTENSION_MESSAGE_ID);
    
    // Handshake ID (0)
    message.push_back(BT_EXTENSION_HANDSHAKE_ID);
    
    // Handshake payload
    message.insert(message.end(), encodedHandshake.begin(), encodedHandshake.end());
    
    return message;
}

bool ExtensionProtocol::processHandshake(const uint8_t* data, size_t length) {
    if (!data || length < 2) {
        logger->error("Invalid extension handshake: too short");
        return false;
    }
    
    // Skip the extension message ID and handshake ID
    const uint8_t* payload = data + 2;
    size_t payloadLength = length - 2;
    
    try {
        // Decode the handshake
        size_t pos = 0;
        auto handshake = bencode::BencodeDecoder::decode(
            std::string(reinterpret_cast<const char*>(payload), payloadLength), pos);
        
        if (!handshake || !handshake->isDictionary()) {
            logger->error("Invalid extension handshake: not a dictionary");
            return false;
        }
        
        // Process extension message IDs
        auto extensions = handshake->get("m");
        if (extensions && extensions->isDictionary()) {
            m_peerExtensions.clear();
            
            for (const auto& [name, value] : extensions->getDictionary()) {
                if (value->isInteger()) {
                    uint8_t id = static_cast<uint8_t>(value->getInteger());
                    if (id > 0) {
                        m_peerExtensions[name] = id;
                        logger->debug("Peer supports extension: {} (ID: {})", name, id);
                    }
                }
            }
        }
        
        // Process metadata size
        auto metadataSize = handshake->getInteger("metadata_size");
        if (metadataSize) {
            m_metadataSize = static_cast<uint32_t>(*metadataSize);
            logger->debug("Peer reported metadata size: {}", m_metadataSize);
        }
        
        // Process client version
        auto clientVersion = handshake->getString("v");
        if (clientVersion) {
            m_clientVersion = *clientVersion;
            logger->debug("Peer client version: {}", m_clientVersion);
        }
        
        // Process peer port
        auto peerPort = handshake->getInteger("p");
        if (peerPort) {
            m_peerPort = static_cast<uint16_t>(*peerPort);
            logger->debug("Peer listening port: {}", m_peerPort);
        }
        
        return true;
    } catch (const bencode::BencodeException& e) {
        logger->error("Failed to decode extension handshake: {}", e.what());
        return false;
    }
}

bool ExtensionProtocol::supportsExtension(const std::string& extensionName) const {
    return m_peerExtensions.find(extensionName) != m_peerExtensions.end();
}

uint8_t ExtensionProtocol::getExtensionId(const std::string& extensionName) const {
    auto it = m_peerExtensions.find(extensionName);
    if (it != m_peerExtensions.end()) {
        return it->second;
    }
    return 0;
}

uint32_t ExtensionProtocol::getMetadataSize() const {
    return m_metadataSize;
}

std::string ExtensionProtocol::getClientVersion() const {
    return m_clientVersion;
}

uint16_t ExtensionProtocol::getPeerPort() const {
    return m_peerPort;
}

std::vector<uint8_t> ExtensionProtocol::createExtensionMessage(
    const std::string& extensionName, const std::vector<uint8_t>& payload) const {
    
    // Get the extension ID
    uint8_t extensionId = getExtensionId(extensionName);
    if (extensionId == 0) {
        logger->error("Cannot create extension message: extension {} not supported by peer", 
                     extensionName);
        return {};
    }
    
    // Create the extension message
    std::vector<uint8_t> message;
    message.reserve(payload.size() + 2);
    
    // Extension message ID
    message.push_back(BT_EXTENSION_MESSAGE_ID);
    
    // Extension ID
    message.push_back(extensionId);
    
    // Payload
    message.insert(message.end(), payload.begin(), payload.end());
    
    return message;
}

bool ExtensionProtocol::processExtensionMessage(const uint8_t* data, size_t length, uint8_t extensionId,
                                              std::string& extensionName, std::vector<uint8_t>& payload) {
    if (!data || length < 2) {
        logger->error("Invalid extension message: too short");
        return false;
    }
    
    // Find the extension name from the ID
    extensionName = "";
    for (const auto& [name, id] : m_peerExtensions) {
        if (id == extensionId) {
            extensionName = name;
            break;
        }
    }
    
    if (extensionName.empty()) {
        logger->warning("Received extension message with unknown ID: {}", extensionId);
        return false;
    }
    
    // Extract the payload
    payload.assign(data + 2, data + length);
    
    return true;
}

} // namespace dht_hunter::metadata
