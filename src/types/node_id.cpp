#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/utility/hash/hash_utils.hpp"
#include <iomanip>
#include <random>
#include <sstream>

namespace dht_hunter::types {

NodeID::NodeID() {
    std::ranges::fill(m_bytes, 0);
}

NodeID::NodeID(const std::array<uint8_t, 20>& bytes) : m_bytes(bytes) {
}

NodeID::NodeID(const std::string& hexString) {
    if (hexString.length() != 40) {
        // Invalid hex string, initialize to zero
        std::ranges::fill(m_bytes, 0);
        return;
    }

    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = hexString.substr(i * 2, 2);
        try {
            m_bytes[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
        } catch (const std::exception&) {
            // Invalid hex string, initialize to zero
            std::ranges::fill(m_bytes, 0);
            return;
        }
    }
}

bool NodeID::operator==(const NodeID& other) const {
    return m_bytes == other.m_bytes;
}

bool NodeID::operator!=(const NodeID& other) const {
    return m_bytes != other.m_bytes;
}

bool NodeID::operator<(const NodeID& other) const {
    return std::ranges::lexicographical_compare(m_bytes, other.m_bytes);
}

uint8_t& NodeID::operator[](const size_t index) {
    return m_bytes[index];
}

uint8_t NodeID::operator[](const size_t index) const {
    return m_bytes[index];
}

size_t NodeID::size() const {
    return m_bytes.size();
}

const std::array<uint8_t, 20>& NodeID::bytes() const {
    return m_bytes;
}

const uint8_t* NodeID::data() const {
    return m_bytes.data();
}

uint8_t* NodeID::data() {
    return m_bytes.data();
}

std::string NodeID::toString() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : m_bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

NodeID NodeID::distanceTo(const NodeID& other) const {
    NodeID distance;
    for (size_t i = 0; i < m_bytes.size(); ++i) {
        distance[i] = m_bytes[i] ^ other.m_bytes[i];
    }
    return distance;
}

bool NodeID::isValid() const {
    return !std::ranges::all_of(m_bytes, [](const uint8_t byte) { return byte == 0; });
}

NodeID NodeID::zero() {
    static const NodeID zeroNodeID;
    return zeroNodeID;
}

NodeID NodeID::random() {
    NodeID nodeID;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : nodeID.m_bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    return nodeID;
}

} // namespace dht_hunter::types
