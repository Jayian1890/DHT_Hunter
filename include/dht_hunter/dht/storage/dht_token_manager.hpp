#pragma once

#include <string>
#include <chrono>
#include <mutex>
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/util/mutex_utils.hpp"

namespace dht_hunter::dht {

/**
 * @brief Manages token generation and validation for DHT operations
 */
class DHTTokenManager {
public:
    /**
     * @brief Constructs a token manager
     */
    DHTTokenManager();

    /**
     * @brief Destructor
     */
    ~DHTTokenManager() = default;

    /**
     * @brief Generates a token for a node
     * 
     * @param endpoint The node's endpoint
     * @return The token
     */
    std::string generateToken(const network::EndPoint& endpoint);

    /**
     * @brief Validates a token for a node
     * 
     * @param token The token
     * @param endpoint The node's endpoint
     * @return true if the token is valid, false otherwise
     */
    bool validateToken(const std::string& token, const network::EndPoint& endpoint);

    /**
     * @brief Rotates the secret
     * 
     * This should be called periodically to maintain security.
     */
    void rotateSecret();

    /**
     * @brief Checks if the secret needs to be rotated
     * 
     * @return true if the secret needs to be rotated, false otherwise
     */
    bool checkSecretRotation();

private:
    std::string m_secret;
    std::chrono::steady_clock::time_point m_secretLastChanged;
    std::string m_previousSecret;
    util::CheckedMutex m_secretMutex;
};

} // namespace dht_hunter::dht
