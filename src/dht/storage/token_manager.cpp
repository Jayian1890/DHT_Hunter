#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/util/hash.hpp"
#include <random>
#include <sstream>
#include <iomanip>

namespace dht_hunter::dht {

TokenManager::TokenManager(const DHTConfig& config)
    : m_config(config), m_running(false), m_logger(event::Logger::forComponent("DHT.TokenManager")) {
    m_logger.info("Creating token manager");

    // Generate initial secrets
    m_currentSecret = generateRandomSecret();
    m_previousSecret = generateRandomSecret();
    m_lastRotation = std::chrono::steady_clock::now();
}

TokenManager::~TokenManager() {
    stop();
}

bool TokenManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Token manager already running");
        return true;
    }

    m_running = true;

    // Start the rotation thread
    m_rotationThread = std::thread(&TokenManager::rotateSecretPeriodically, this);

    m_logger.info("Token manager started");
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

    m_logger.info("Token manager stopped");
}

bool TokenManager::isRunning() const {
    return m_running;
}

std::string TokenManager::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);

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
    std::string previousToken = computeToken(endpoint, m_previousSecret);
    if (token == previousToken) {
        return true;
    }

    return false;
}

void TokenManager::rotateSecret() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Rotate the secrets
    m_previousSecret = m_currentSecret;
    m_currentSecret = generateRandomSecret();
    m_lastRotation = std::chrono::steady_clock::now();

    m_logger.debug("Rotated token secret");
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
    std::string data = endpoint.getAddress().toString() + ":" + std::to_string(endpoint.getPort()) + ":" + secret;

    // Compute the SHA-1 hash
    auto hash = util::sha1(data);

    // Convert the hash to a hex string
    return util::bytesToHex(hash.data(), hash.size());
}

} // namespace dht_hunter::dht
