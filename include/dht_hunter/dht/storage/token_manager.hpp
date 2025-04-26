#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/event/logger.hpp"
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>

namespace dht_hunter::dht {

/**
 * @brief Manages tokens for DHT security (Singleton)
 */
class TokenManager {
public:
    /**
     * @brief Gets the singleton instance of the token manager
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<TokenManager> getInstance(const DHTConfig& config);

    /**
     * @brief Destructor
     */
    ~TokenManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    TokenManager(const TokenManager&) = delete;
    TokenManager& operator=(const TokenManager&) = delete;
    TokenManager(TokenManager&&) = delete;
    TokenManager& operator=(TokenManager&&) = delete;

    /**
     * @brief Starts the token manager
     * @return True if the token manager was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the token manager
     */
    void stop();

    /**
     * @brief Checks if the token manager is running
     * @return True if the token manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Generates a token for an endpoint
     * @param endpoint The endpoint
     * @return The token
     */
    std::string generateToken(const network::EndPoint& endpoint);

    /**
     * @brief Verifies a token for an endpoint
     * @param token The token
     * @param endpoint The endpoint
     * @return True if the token is valid, false otherwise
     */
    bool verifyToken(const std::string& token, const network::EndPoint& endpoint);

private:
    /**
     * @brief Rotates the secret
     */
    void rotateSecret();

    /**
     * @brief Rotates the secret periodically
     */
    void rotateSecretPeriodically();

    /**
     * @brief Generates a random secret
     * @return The secret
     */
    std::string generateRandomSecret();

    /**
     * @brief Computes a token for an endpoint and a secret
     * @param endpoint The endpoint
     * @param secret The secret
     * @return The token
     */
    std::string computeToken(const network::EndPoint& endpoint, const std::string& secret);

    /**
     * @brief Private constructor for singleton pattern
     */
    explicit TokenManager(const DHTConfig& config);

    // Static instance for singleton pattern
    static std::shared_ptr<TokenManager> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    std::string m_currentSecret;
    std::string m_previousSecret;
    std::chrono::steady_clock::time_point m_lastRotation;
    std::atomic<bool> m_running;
    std::thread m_rotationThread;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
