#pragma once

/**
 * @file utility.hpp
 * @brief Main utility header that includes all consolidated utility modules
 *
 * This file provides access to all utility functions in the BitScrape project.
 * It includes the consolidated utility modules and provides backward compatibility
 * with the legacy utility namespace structure.
 */

// Include consolidated utility modules
#include "utils/common_utils.hpp"
#include "utils/domain_utils.hpp"
#include "utils/system_utils.hpp"

// Include remaining legacy utility modules
#include "dht_hunter/utility/random/random_utils.hpp"
#include "dht_hunter/utility/json/json.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"

// Include legacy utils header for backward compatibility
#include "dht_hunter/utility/legacy_utils.hpp"

// Backward compatibility namespace aliases
namespace dht_hunter::utility {
    // String utilities
    namespace string {
        using dht_hunter::utility::string::bytesToHex;
        using dht_hunter::utility::string::hexToBytes;
        using dht_hunter::utility::string::formatHex;
        using dht_hunter::utility::string::truncateString;
    }

    // Hash utilities
    namespace hash {
        using dht_hunter::utility::hash::sha1;
        using dht_hunter::utility::hash::sha1Hex;
    }

    // File utilities
    namespace file {
        using dht_hunter::utility::file::fileExists;
        using dht_hunter::utility::file::createDirectory;
        using dht_hunter::utility::file::getFileSize;
        using dht_hunter::utility::file::readFile;
        using dht_hunter::utility::file::writeFile;
    }

    // Filesystem utilities
    namespace filesystem {
        using dht_hunter::utility::filesystem::getFiles;
        using dht_hunter::utility::filesystem::getDirectories;
        using dht_hunter::utility::filesystem::getFileExtension;
        using dht_hunter::utility::filesystem::getFileName;
        using dht_hunter::utility::filesystem::getDirectoryPath;
    }

    // Network utilities
    namespace network {
        using dht_hunter::utility::network::getUserAgent;
    }

    // Node utilities
    namespace node {
        using dht_hunter::utility::node::sortNodesByDistance;
        using dht_hunter::utility::node::findNodeByEndpoint;
        using dht_hunter::utility::node::findNodeByID;
        using dht_hunter::utility::node::generateNodeLookupID;
        using dht_hunter::utility::node::generatePeerLookupID;
    }

    // Metadata utilities
    namespace metadata {
        using dht_hunter::utility::metadata::setInfoHashName;
        using dht_hunter::utility::metadata::getInfoHashName;
        using dht_hunter::utility::metadata::addFileToInfoHash;
        using dht_hunter::utility::metadata::getInfoHashFiles;
        using dht_hunter::utility::metadata::getInfoHashTotalSize;
        using dht_hunter::utility::metadata::getInfoHashMetadata;
        using dht_hunter::utility::metadata::getAllMetadata;
    }

    // Thread utilities
    namespace thread {
        using dht_hunter::utility::system::thread::LockTimeoutException;
        using dht_hunter::utility::system::thread::g_shuttingDown;
        using dht_hunter::utility::system::thread::withLock;
        using dht_hunter::utility::system::thread::runAsync;
        using dht_hunter::utility::system::thread::sleep;
        using dht_hunter::utility::system::thread::ThreadPool;
    }

    // Process utilities
    namespace process {
        using dht_hunter::utility::system::process::getMemoryUsage;
        using dht_hunter::utility::system::process::formatSize;
    }

    // Memory utilities
    namespace system {
        using dht_hunter::utility::system::memory::getTotalSystemMemory;
        using dht_hunter::utility::system::memory::getAvailableSystemMemory;
        using dht_hunter::utility::system::memory::calculateMaxTransactions;
    }
}
