#include "dht_hunter/utility/system/memory_utils.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <algorithm>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace dht_hunter::utility::system {

uint64_t getTotalSystemMemory() {
    uint64_t totalMemory = 0;

    try {
#ifdef __APPLE__
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        size_t length = sizeof(totalMemory);
        if (sysctl(mib, 2, &totalMemory, &length, NULL, 0) != 0) {
            unified_event::logError("Utility.System", "Failed to get total system memory");
            return 0;
        }
#elif defined(_WIN32)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            totalMemory = memInfo.ullTotalPhys;
        } else {
            unified_event::logError("Utility.System", "Failed to get total system memory");
            return 0;
        }
#else
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            totalMemory = static_cast<uint64_t>(memInfo.totalram) * memInfo.mem_unit;
        } else {
            unified_event::logError("Utility.System", "Failed to get total system memory");
            return 0;
        }
#endif
    } catch (const std::exception& e) {
        unified_event::logError("Utility.System", "Exception getting total system memory: " + std::string(e.what()));
        return 0;
    } catch (...) {
        unified_event::logError("Utility.System", "Unknown exception getting total system memory");
        return 0;
    }

    return totalMemory;
}

uint64_t getAvailableSystemMemory() {
    uint64_t availableMemory = 0;

    try {
#ifdef __APPLE__
        mach_port_t host_port = mach_host_self();
        mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
        vm_size_t page_size;
        vm_statistics64_data_t vm_stats;

        host_page_size(host_port, &page_size);
        if (host_statistics64(host_port, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stats), &host_size) == KERN_SUCCESS) {
            availableMemory = static_cast<uint64_t>(vm_stats.free_count + vm_stats.inactive_count) * page_size;
        } else {
            unified_event::logError("Utility.System", "Failed to get available system memory");
            return 0;
        }
#elif defined(_WIN32)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            availableMemory = memInfo.ullAvailPhys;
        } else {
            unified_event::logError("Utility.System", "Failed to get available system memory");
            return 0;
        }
#else
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            availableMemory = static_cast<uint64_t>(memInfo.freeram + memInfo.bufferram) * memInfo.mem_unit;
        } else {
            unified_event::logError("Utility.System", "Failed to get available system memory");
            return 0;
        }
#endif
    } catch (const std::exception& e) {
        unified_event::logError("Utility.System", "Exception getting available system memory: " + std::string(e.what()));
        return 0;
    } catch (...) {
        unified_event::logError("Utility.System", "Unknown exception getting available system memory");
        return 0;
    }

    return availableMemory;
}

size_t calculateMaxTransactions(
    double percentageOfMemory,
    size_t bytesPerTransaction,
    size_t minTransactions,
    size_t maxTransactions) {

    // Ensure percentage is within valid range
    percentageOfMemory = std::clamp(percentageOfMemory, 0.01, 0.9);

    // Get available memory
    uint64_t availableMemory = getAvailableSystemMemory();

    // If we couldn't determine available memory, use a conservative default
    if (availableMemory == 0) {
        unified_event::logWarning("Utility.System", "Could not determine available memory, using default max transactions");
        return minTransactions;
    }

    // Calculate memory to use for transactions
    uint64_t memoryForTransactions = static_cast<uint64_t>(availableMemory * percentageOfMemory);

    // Calculate max transactions based on memory
    size_t calculatedMax = static_cast<size_t>(memoryForTransactions / bytesPerTransaction);

    // Clamp to min/max range
    size_t result = std::clamp(calculatedMax, minTransactions, maxTransactions);

    unified_event::logInfo("Utility.System",
        "Available memory: " + std::to_string(availableMemory / (1024 * 1024)) + " MB, " +
        "Using " + std::to_string(static_cast<int>(percentageOfMemory * 100)) + "% for transactions: " +
        std::to_string(memoryForTransactions / (1024 * 1024)) + " MB, " +
        "Max transactions: " + std::to_string(result));

    return result;
}

} // namespace dht_hunter::utility::system
