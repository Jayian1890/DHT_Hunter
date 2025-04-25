#include "dht_hunter/dht/storage/dht_token_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "TokenManager")

namespace {
// Helper function to generate a random string of the specified length
std::string generateRandomString(size_t length) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[dist(rng)];
    }
    return result;
}
}

namespace dht_hunter::dht {

DHTTokenManager::DHTTokenManager()
    : m_secret(generateRandomString(DEFAULT_SECRET_LENGTH)),
      m_secretLastChanged(std::chrono::steady_clock::now()),
      m_previousSecret("") {
    getLogger()->debug("Token manager initialized with new secret");
}

std::string DHTTokenManager::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

bool DHTTokenManager::validateToken(const std::string& token, const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and current secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    // Check if token matches
    if (token == ss.str()) {
        return true;
    }
    // If not, try with previous secret
    if (!m_previousSecret.empty()) {
        ss.str("");
        ss << endpoint.toString() << m_previousSecret;
        hash = hasher(ss.str());
        ss.str("");
        ss << std::hex << std::setw(16) << std::setfill('0') << hash;
        return token == ss.str();
    }
    return false;
}

void DHTTokenManager::rotateSecret() {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    m_previousSecret = m_secret;
    m_secret = generateRandomString(DEFAULT_SECRET_LENGTH);
    m_secretLastChanged = std::chrono::steady_clock::now();
    getLogger()->debug("Secret rotated");
}

bool DHTTokenManager::checkSecretRotation() {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_secretLastChanged).count();
    return elapsed >= DEFAULT_SECRET_ROTATION_INTERVAL_MINUTES;
}

} // namespace dht_hunter::dht
