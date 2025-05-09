#pragma once

/**
 * @file process_utils.hpp
 * @brief Legacy process utilities header that forwards to the new consolidated utilities
 *
 * This file provides backward compatibility for legacy code that still includes
 * the old process utilities header. It forwards to the new consolidated utilities.
 *
 * @deprecated Use utils/system_utils.hpp instead
 */

#include "dht_hunter/utility/legacy_utils.hpp"

namespace dht_hunter::utility::process {

/**
 * @brief Utility functions for process information
 * @deprecated Use the free functions in dht_hunter::utility::system::process namespace instead
 */
class ProcessUtils {
public:
    /**
     * @brief Gets the current process memory usage in bytes
     * @return The memory usage in bytes
     */
    static uint64_t getMemoryUsage() {
        return dht_hunter::utility::system::process::getMemoryUsage();
    }

    /**
     * @brief Formats a size in bytes to a human-readable string
     * @param bytes The size in bytes
     * @return A human-readable string (e.g., "1.23 MB")
     */
    static std::string formatSize(uint64_t bytes) {
        return dht_hunter::utility::system::process::formatSize(bytes);
    }
};

} // namespace dht_hunter::utility::process
