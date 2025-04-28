#pragma once

#include <cstdint>
#include <string>

namespace dht_hunter::utility::process {

/**
 * @brief Utility functions for process information
 */
class ProcessUtils {
public:
    /**
     * @brief Gets the current process memory usage in bytes
     * @return The memory usage in bytes
     */
    static uint64_t getMemoryUsage();

    /**
     * @brief Formats a size in bytes to a human-readable string
     * @param bytes The size in bytes
     * @return A human-readable string (e.g., "1.23 MB")
     */
    static std::string formatSize(uint64_t bytes);
};

} // namespace dht_hunter::utility::process
