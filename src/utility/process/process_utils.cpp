#include "dht_hunter/utility/process/process_utils.hpp"
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#endif

namespace dht_hunter::utility::process {

uint64_t ProcessUtils::getMemoryUsage() {
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
    } catch (const std::exception&) {
        // Silently handle exceptions
    } catch (...) {
        // Silently handle unknown exceptions
    }

    return 0;
}

std::string ProcessUtils::formatSize(uint64_t bytes) {
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

} // namespace dht_hunter::utility::process
