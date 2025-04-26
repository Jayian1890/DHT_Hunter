#include "dht_hunter/util/hash.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace dht_hunter::util {

// Helper function for SHA-1 algorithm
inline uint32_t leftRotate(uint32_t value, uint32_t bits) {
    return ((value << bits) | (value >> (32 - bits)));
}
void sha1(const uint8_t* data, size_t length, uint8_t* output) {
    // Initialize hash values (FIPS 180-4)
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;
    // Pre-processing: padding the message
    // Calculate how many complete 64-byte blocks we can process
    size_t initialLen = length;
    size_t numBlocks = ((initialLen + 8) / 64) + 1;
    size_t totalLen = numBlocks * 64;
    // Create a buffer for the padded message
    std::vector<uint8_t> paddedMessage(totalLen, 0);
    // Copy the original message
    std::copy(data, data + initialLen, paddedMessage.begin());
    // Append the bit '1' to the message
    paddedMessage[initialLen] = 0x80;
    // Append the length in bits as a 64-bit big-endian integer
    uint64_t bitLength = initialLen * 8;
    for (size_t i = 0; i < 8; ++i) {
        paddedMessage[totalLen - 8UL + i] = static_cast<uint8_t>((bitLength >> (56UL - i * 8UL)) & 0xFFUL);
    }
    // Process the message in 512-bit (64-byte) chunks
    for (size_t chunk = 0; chunk < numBlocks; ++chunk) {
        uint32_t w[80];
        // Break chunk into sixteen 32-bit big-endian words
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(paddedMessage[chunk * 64UL + i * 4UL]) << 24U) |
                   (static_cast<uint32_t>(paddedMessage[chunk * 64UL + i * 4UL + 1UL]) << 16U) |
                   (static_cast<uint32_t>(paddedMessage[chunk * 64UL + i * 4UL + 2UL]) << 8U) |
                   static_cast<uint32_t>(paddedMessage[chunk * 64UL + i * 4UL + 3UL]);
        }
        // Extend the sixteen 32-bit words into eighty 32-bit words
        for (size_t i = 16; i < 80; ++i) {
            w[i] = leftRotate(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1U);
        }
        // Initialize hash value for this chunk
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;
        // Main loop
        for (size_t i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999U;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1U;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCU;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6U;
            }
            uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }
        // Add this chunk's hash to result
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    // Produce the final hash value as a 160-bit number (20 bytes)
    output[0] = (h0 >> 24) & 0xFF;
    output[1] = (h0 >> 16) & 0xFF;
    output[2] = (h0 >> 8) & 0xFF;
    output[3] = h0 & 0xFF;
    output[4] = (h1 >> 24) & 0xFF;
    output[5] = (h1 >> 16) & 0xFF;
    output[6] = (h1 >> 8) & 0xFF;
    output[7] = h1 & 0xFF;
    output[8] = (h2 >> 24) & 0xFF;
    output[9] = (h2 >> 16) & 0xFF;
    output[10] = (h2 >> 8) & 0xFF;
    output[11] = h2 & 0xFF;
    output[12] = (h3 >> 24) & 0xFF;
    output[13] = (h3 >> 16) & 0xFF;
    output[14] = (h3 >> 8) & 0xFF;
    output[15] = h3 & 0xFF;
    output[16] = (h4 >> 24) & 0xFF;
    output[17] = (h4 >> 16) & 0xFF;
    output[18] = (h4 >> 8) & 0xFF;
    output[19] = h4 & 0xFF;
}
std::array<uint8_t, 20> sha1(const std::vector<uint8_t>& data) {
    std::array<uint8_t, 20> hash;
    sha1(data.data(), data.size(), hash.data());
    return hash;
}
std::array<uint8_t, 20> sha1(const std::string& data) {
    std::array<uint8_t, 20> hash;
    sha1(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hash.data());
    return hash;
}
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
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
            bytes.push_back(byte);
        }
    }
    return bytes;
}

} // namespace dht_hunter::util
