#pragma once

#include <cstdint>
#include <cstddef>

namespace dht_hunter::utility::system {

/**
 * @brief Gets the total system memory in bytes
 * @return The total system memory in bytes, or 0 if it couldn't be determined
 */
uint64_t getTotalSystemMemory();

/**
 * @brief Gets the available system memory in bytes
 * @return The available system memory in bytes, or 0 if it couldn't be determined
 */
uint64_t getAvailableSystemMemory();

/**
 * @brief Calculates the maximum number of transactions based on available memory
 * @param percentageOfMemory The percentage of available memory to use (0.0-1.0)
 * @param bytesPerTransaction The estimated bytes per transaction
 * @param minTransactions The minimum number of transactions to allow
 * @param maxTransactions The maximum number of transactions to allow
 * @return The calculated maximum number of transactions
 */
size_t calculateMaxTransactions(
    double percentageOfMemory = 0.25,
    size_t bytesPerTransaction = 350,
    size_t minTransactions = 1000,
    size_t maxTransactions = 1000000);

} // namespace dht_hunter::utility::system
