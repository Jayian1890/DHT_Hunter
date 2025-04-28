#include "dht_hunter/types/info_hash.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace dht_hunter::types {

std::string infoHashToString(const InfoHash& infoHash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

bool infoHashFromString(const std::string& str, InfoHash& infoHash) {
    // Check if the string has the correct length (40 characters for 20 bytes in hex)
    if (str.length() != 40) {
        return false;
    }

    // Convert each pair of hex characters to a byte
    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = str.substr(i * 2, 2);
        try {
            infoHash[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
        } catch (const std::exception&) {
            return false;
        }
    }

    return true;
}

InfoHash createEmptyInfoHash() {
    InfoHash emptyHash{}; // Initialize with all zeros
    return emptyHash;
}

bool isValidInfoHash(const InfoHash& infoHash) {
    // Check if the info hash is all zeros
    return !std::ranges::all_of(infoHash, [](const uint8_t byte) { return byte == 0; });
}

} // namespace dht_hunter::types
