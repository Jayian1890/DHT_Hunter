#include "dht_hunter/utility/string/string_utils.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::utility::string {

std::string bytesToHex(const uint8_t* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    
    // Check if the string has an even length
    if (hex.length() % 2 != 0) {
        return bytes;
    }
    
    // Convert each pair of hex characters to a byte
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
        } catch (const std::exception&) {
            // Invalid hex string
            bytes.clear();
            return bytes;
        }
    }
    
    return bytes;
}

std::string formatHex(const uint8_t* data, size_t length, const std::string& separator) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < length; ++i) {
        if (i > 0 && !separator.empty()) {
            ss << separator;
        }
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

std::string truncateString(const std::string& str, size_t maxLength) {
    if (str.length() <= maxLength) {
        return str;
    }
    
    return str.substr(0, maxLength) + "...";
}

} // namespace dht_hunter::utility::string
