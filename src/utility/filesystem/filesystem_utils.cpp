#include "dht_hunter/utility/filesystem/filesystem_utils.hpp"

#include <fstream>
#include <array>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

// No logging in utility functions

namespace dht_hunter::utility::filesystem {

bool FilesystemUtils::ensureDirectoryExists(
    const std::filesystem::path& path,
    const std::function<void(const std::string&, const std::string&)>& loggerCallback) {

    try {
        // If the path already exists and is a directory, we're done
        if (std::filesystem::exists(path)) {
            if (std::filesystem::is_directory(path)) {
                if (loggerCallback) {
                    loggerCallback("debug", "Directory already exists: " + path.string());
                }
                return true;
            } else {
                // Path exists but is not a directory
                if (loggerCallback) {
                    loggerCallback("error", "Path exists but is not a directory: " + path.string());
                }
                return false;
            }
        }

        // Create the directory
        if (loggerCallback) {
            loggerCallback("info", "Creating directory: " + path.string());
        }

        if (!std::filesystem::create_directories(path)) {
            if (loggerCallback) {
                loggerCallback("error", "Failed to create directory: " + path.string());
            }
            return false;
        }

        if (loggerCallback) {
            loggerCallback("debug", "Created directory: " + path.string());
        }

        return true;
    } catch (const std::exception& e) {
        if (loggerCallback) {
            loggerCallback("error", "Exception while creating directory " + path.string() + ": " + e.what());
        }
        return false;
    }
}

bool FilesystemUtils::ensureParentDirectoryExists(
    const std::filesystem::path& filePath,
    const std::function<void(const std::string&, const std::string&)>& loggerCallback) {

    std::filesystem::path parentPath = filePath.parent_path();
    if (parentPath.empty()) {
        // No parent directory (file is in current directory)
        return true;
    }

    return ensureDirectoryExists(parentPath, loggerCallback);
}

bool FilesystemUtils::isDirectoryWritable(
    const std::filesystem::path& path,
    const std::function<void(const std::string&, const std::string&)>& loggerCallback) {

    try {
        // Ensure the directory exists first
        if (!ensureDirectoryExists(path, loggerCallback)) {
            return false;
        }

        // Try to create a test file
        std::filesystem::path testFilePath = path / "test.tmp";

        if (loggerCallback) {
            loggerCallback("debug", "Testing if directory is writable: " + path.string());
        }

        std::ofstream testFile(testFilePath);
        if (!testFile) {
            if (loggerCallback) {
                loggerCallback("error", "Directory is not writable: " + path.string());
            }
            return false;
        }
        testFile.close();

        // Remove the test file
        std::filesystem::remove(testFilePath);

        if (loggerCallback) {
            loggerCallback("debug", "Directory is writable: " + path.string());
        }

        return true;
    } catch (const std::exception& e) {
        if (loggerCallback) {
            loggerCallback("error", "Exception while testing directory writability " + path.string() + ": " + e.what());
        }
        return false;
    }
}

std::optional<std::filesystem::path> FilesystemUtils::getExecutablePath() {
    try {
#ifdef _WIN32
        // Windows implementation
        std::array<char, MAX_PATH> buffer;
        if (GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) == 0) {
            // Failed to get executable path
            return std::nullopt;
        }
        return std::filesystem::path(buffer.data());
#elif defined(__APPLE__)
        // macOS implementation
        std::array<char, PATH_MAX> buffer;
        uint32_t size = buffer.size();
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            // Failed to get executable path
            return std::nullopt;
        }
        return std::filesystem::path(buffer.data());
#else
        // Linux implementation
        std::array<char, PATH_MAX> buffer;
        ssize_t count = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (count <= 0) {
            // Failed to get executable path
            return std::nullopt;
        }
        buffer[count] = '\0';
        return std::filesystem::path(buffer.data());
#endif
    } catch (const std::exception& e) {
        // Exception while getting executable path
        return std::nullopt;
    }
}

std::string FilesystemUtils::getExecutableName() {
    auto execPath = getExecutablePath();
    if (!execPath) {
        return "dht_hunter"; // Default fallback name
    }

    // Get the filename without extension
    std::string filename = execPath->filename().string();

    // Remove extension if present
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }

    return filename;
}

void FilesystemUtils::setTerminalTitle(const std::string& title) {
    // Use ANSI escape sequence to set the terminal title
    // This works on most modern terminals including Windows 10+ with modern console
    std::cout << "\033]0;" << title << "\007" << std::flush;
}

} // namespace dht_hunter::utility::filesystem
