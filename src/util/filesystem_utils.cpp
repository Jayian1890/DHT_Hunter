#include "dht_hunter/util/filesystem_utils.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

#include <fstream>

DEFINE_COMPONENT_LOGGER("Util", "FilesystemUtils")

namespace dht_hunter::util {

bool FilesystemUtils::ensureDirectoryExists(
    const std::filesystem::path& path,
    const std::function<void(const std::string&, const std::string&)>& loggerCallback) {
    
    try {
        // If the path already exists and is a directory, we're done
        if (std::filesystem::exists(path)) {
            if (std::filesystem::is_directory(path)) {
                if (loggerCallback) {
                    loggerCallback("debug", "Directory already exists: " + path.string());
                } else {
                    getLogger()->debug("Directory already exists: {}", path.string());
                }
                return true;
            } else {
                // Path exists but is not a directory
                if (loggerCallback) {
                    loggerCallback("error", "Path exists but is not a directory: " + path.string());
                } else {
                    getLogger()->error("Path exists but is not a directory: {}", path.string());
                }
                return false;
            }
        }
        
        // Create the directory
        if (loggerCallback) {
            loggerCallback("info", "Creating directory: " + path.string());
        } else {
            getLogger()->info("Creating directory: {}", path.string());
        }
        
        if (!std::filesystem::create_directories(path)) {
            if (loggerCallback) {
                loggerCallback("error", "Failed to create directory: " + path.string());
            } else {
                getLogger()->error("Failed to create directory: {}", path.string());
            }
            return false;
        }
        
        if (loggerCallback) {
            loggerCallback("debug", "Created directory: " + path.string());
        } else {
            getLogger()->debug("Created directory: {}", path.string());
        }
        
        return true;
    } catch (const std::exception& e) {
        if (loggerCallback) {
            loggerCallback("error", "Exception while creating directory " + path.string() + ": " + e.what());
        } else {
            getLogger()->error("Exception while creating directory {}: {}", path.string(), e.what());
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
        } else {
            getLogger()->debug("Testing if directory is writable: {}", path.string());
        }
        
        std::ofstream testFile(testFilePath);
        if (!testFile) {
            if (loggerCallback) {
                loggerCallback("error", "Directory is not writable: " + path.string());
            } else {
                getLogger()->error("Directory is not writable: {}", path.string());
            }
            return false;
        }
        testFile.close();
        
        // Remove the test file
        std::filesystem::remove(testFilePath);
        
        if (loggerCallback) {
            loggerCallback("debug", "Directory is writable: " + path.string());
        } else {
            getLogger()->debug("Directory is writable: {}", path.string());
        }
        
        return true;
    } catch (const std::exception& e) {
        if (loggerCallback) {
            loggerCallback("error", "Exception while testing directory writability " + path.string() + ": " + e.what());
        } else {
            getLogger()->error("Exception while testing directory writability {}: {}", path.string(), e.what());
        }
        return false;
    }
}

} // namespace dht_hunter::util