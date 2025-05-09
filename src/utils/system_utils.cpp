/**
 * @file system_utils.cpp
 * @brief Implementation of system utility functions
 */

#include "utils/system_utils.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#endif

namespace dht_hunter::utility::system {

//-----------------------------------------------------------------------------
// Thread utilities implementation
//-----------------------------------------------------------------------------
namespace thread {

// Initialize the global shutdown flag
std::atomic<bool> g_shuttingDown(false);

ThreadPool::ThreadPool(size_t numThreads) : m_stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    
                    // Wait for a task or stop signal
                    m_condition.wait(lock, [this] {
                        return m_stop || !m_tasks.empty();
                    });
                    
                    // Exit if stopped and no tasks
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    
                    // Get the next task
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                
                // Execute the task
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    
    // Notify all threads to check the stop flag
    m_condition.notify_all();
    
    // Join all threads
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace thread

//-----------------------------------------------------------------------------
// Process utilities implementation
//-----------------------------------------------------------------------------
namespace process {

uint64_t getMemoryUsage() {
    try {
#ifdef _WIN32
        // Windows implementation
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
#elif defined(__APPLE__)
        // macOS implementation
        struct task_basic_info t_info;
        mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

        if (task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&t_info), &t_info_count) == KERN_SUCCESS) {
            return t_info.resident_size;
        }
#else
        // Linux implementation
        long rss = 0L;
        FILE* fp = NULL;
        if ((fp = fopen("/proc/self/statm", "r")) == NULL) {
            return 0;
        }
        if (fscanf(fp, "%*s%ld", &rss) != 1) {
            fclose(fp);
            return 0;
        }
        fclose(fp);
        return (uint64_t)rss * (uint64_t)sysconf(_SC_PAGESIZE);
#endif
    } catch (const std::exception& e) {
        unified_event::logError("Utility.Process", "Exception getting process memory usage: " + std::string(e.what()));
    } catch (...) {
        unified_event::logError("Utility.Process", "Unknown exception getting process memory usage");
    }

    return 0;
}

std::string formatSize(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

} // namespace process

//-----------------------------------------------------------------------------
// Memory utilities implementation
//-----------------------------------------------------------------------------
namespace memory {

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

} // namespace memory

} // namespace dht_hunter::utility::system
