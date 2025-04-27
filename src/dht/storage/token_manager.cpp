#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/util/hash.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<TokenManager> TokenManager::s_instance = nullptr;
std::mutex TokenManager::s_instanceMutex;

std::shared_ptr<TokenManager> TokenManager::getInstance(const DHTConfig& config) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<TokenManager>(new TokenManager(config));
    }

    return s_instance;
}

TokenManager::TokenManager(const DHTConfig& config)
    : m_config(config), m_running(false) {
    // Generate initial secrets
    m_currentSecret = generateRandomSecret();
    m_previousSecret = generateRandomSecret();
    m_lastRotation = std::chrono::steady_clock::now();
}

TokenManager::~TokenManager() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool TokenManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    m_running = true;

    // Start the rotation thread
    m_rotationThread = std::thread(&TokenManager::rotateSecretPeriodically, this);

    return true;
}

void TokenManager::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running) {
            return;
        }

        m_running = false;
    } // Release the lock before joining the thread

    // Wait for the rotation thread to finish
    if (m_rotationThread.joinable()) {
        m_rotationThread.join();
    }
}

bool TokenManager::isRunning() const {
    return m_running;
}

std::string TokenManager::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a token using the current secret
    // As per BEP-5, tokens are generated using the SHA1 hash of the IP address concatenated with a secret
    return computeToken(endpoint, m_currentSecret);
}

bool TokenManager::verifyToken(const std::string& token, const network::EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the token matches the current secret
    std::string currentToken = computeToken(endpoint, m_currentSecret);
    if (token == currentToken) {
        return true;
    }

    // Check if the token matches the previous secret
    // As per BEP-5, tokens must be accepted for a reasonable amount of time after they have been distributed
    // BEP-5 recommends accepting tokens up to 10 minutes old
    std::string previousToken = computeToken(endpoint, m_previousSecret);
    if (token == previousToken) {
        return true;
    }

    return false;
}

void TokenManager::rotateSecret() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Rotate the secrets
    // Keep the previous secret to verify tokens that were issued with it
    // As per BEP-5, tokens up to 10 minutes old should be accepted
    m_previousSecret = m_currentSecret;
    m_currentSecret = generateRandomSecret();
    m_lastRotation = std::chrono::steady_clock::now();
}

void TokenManager::rotateSecretPeriodically() {
    while (m_running) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if it's time to rotate the secret
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRotation).count();

        if (elapsed >= m_config.getTokenRotationInterval()) {
            rotateSecret();
        }
    }
}

std::string TokenManager::generateRandomSecret() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    // Generate a random secret
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < 20; ++i) {
        ss << std::setw(2) << dis(gen);
    }

    return ss.str();
}

std::string TokenManager::computeToken(const network::EndPoint& endpoint, const std::string& secret) {
    // Compute a token based on the endpoint and the secret
    // As per BEP-5, the token is the SHA1 hash of the IP address concatenated with a secret
    // We include the port as well for additional security
    std::string data = endpoint.getAddress().toString() + ":" + std::to_string(endpoint.getPort()) + ":" + secret;

    // Compute the SHA-1 hash
    auto hash = util::sha1(data);

    // Convert the hash to a hex string
    return util::bytesToHex(hash.data(), hash.size());
}

} // namespace dht_hunter::dht
