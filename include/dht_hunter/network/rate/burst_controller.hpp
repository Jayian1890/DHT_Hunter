#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace dht_hunter::network {

/**
 * @class BurstController
 * @brief Controls burst traffic to prevent flooding.
 *
 * This class provides a mechanism for controlling burst traffic to specific endpoints,
 * to prevent flooding or denial-of-service attacks.
 */
class BurstController {
public:
    /**
     * @brief Constructs a burst controller.
     * @param maxBurstSize The maximum number of operations allowed in a burst.
     * @param burstWindow The time window for a burst.
     */
    BurstController(size_t maxBurstSize = 10, std::chrono::milliseconds burstWindow = std::chrono::milliseconds(1000));

    /**
     * @brief Destructor.
     */
    ~BurstController();

    /**
     * @brief Tries to acquire permission for an operation to an endpoint.
     * @param endpoint The endpoint to operate on.
     * @return True if permission was granted, false otherwise.
     */
    bool tryAcquire(const EndPoint& endpoint);

    /**
     * @brief Gets the maximum number of operations allowed in a burst.
     * @return The maximum number of operations allowed in a burst.
     */
    size_t getMaxBurstSize() const;

    /**
     * @brief Sets the maximum number of operations allowed in a burst.
     * @param maxBurstSize The maximum number of operations allowed in a burst.
     */
    void setMaxBurstSize(size_t maxBurstSize);

    /**
     * @brief Gets the time window for a burst.
     * @return The time window for a burst.
     */
    std::chrono::milliseconds getBurstWindow() const;

    /**
     * @brief Sets the time window for a burst.
     * @param burstWindow The time window for a burst.
     */
    void setBurstWindow(std::chrono::milliseconds burstWindow);

    /**
     * @brief Gets the number of operations performed to an endpoint in the current burst window.
     * @param endpoint The endpoint to check.
     * @return The number of operations performed.
     */
    size_t getBurstCount(const EndPoint& endpoint) const;

    /**
     * @brief Clears all burst counts.
     */
    void clear();

private:
    /**
     * @brief Removes expired burst counts.
     */
    void removeExpiredBursts();

    struct Burst {
        size_t count;
        std::chrono::steady_clock::time_point timestamp;
    };

    size_t m_maxBurstSize;
    std::chrono::milliseconds m_burstWindow;
    std::unordered_map<EndPoint, Burst> m_bursts;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
