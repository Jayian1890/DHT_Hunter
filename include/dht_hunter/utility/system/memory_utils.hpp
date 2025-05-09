#pragma once

/**
 * @file memory_utils.hpp
 * @brief Legacy memory utilities header that forwards to the new consolidated utilities
 *
 * This file provides backward compatibility for legacy code that still includes
 * the old memory utilities header. It forwards to the new consolidated utilities.
 *
 * @deprecated Use utils/system_utils.hpp instead
 */

#include "dht_hunter/utility/legacy_utils.hpp"

namespace dht_hunter::utility::system {

/**
 * @brief Gets the total system memory in bytes
 * @return The total system memory in bytes, or 0 if it couldn't be determined
 */
inline uint64_t getTotalSystemMemory() {
    return dht_hunter::utility::system::memory::getTotalSystemMemory();
}

/**
 * @brief Gets the available system memory in bytes
 * @return The available system memory in bytes, or 0 if it couldn't be determined
 */
inline uint64_t getAvailableSystemMemory() {
    return dht_hunter::utility::system::memory::getAvailableSystemMemory();
}

/**
 * @brief Calculates the maximum number of transactions based on available memory
 * @param percentageOfMemory The percentage of available memory to use (0.0-1.0)
 * @param bytesPerTransaction The estimated bytes per transaction
 * @param minTransactions The minimum number of transactions to allow
 * @param maxTransactions The maximum number of transactions to allow
 * @return The calculated maximum number of transactions
 */
inline size_t calculateMaxTransactions(
    double percentageOfMemory = 0.25,
    size_t bytesPerTransaction = 350,
    size_t minTransactions = 1000,
    size_t maxTransactions = 1000000) {
    return dht_hunter::utility::system::memory::calculateMaxTransactions(
        percentageOfMemory, bytesPerTransaction, minTransactions, maxTransactions);
}

} // namespace dht_hunter::utility::system
