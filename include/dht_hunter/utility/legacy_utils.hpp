#pragma once

/**
 * @file legacy_utils.hpp
 * @brief Legacy utility header that forwards to the new consolidated utilities
 *
 * This file provides backward compatibility for legacy code that still includes
 * the old utility headers. It forwards to the new consolidated utilities.
 */

// Include the new consolidated utilities
#include "utils/common_utils.hpp"
#include "utils/domain_utils.hpp"
#include "utils/system_utils.hpp"
#include "utils/misc_utils.hpp"
#include "utils/config_utils.hpp"

// Forward declarations for network utilities
namespace dht_hunter::utility::network {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::network::getUserAgent;
}

// Forward declarations for node utilities
namespace dht_hunter::utility::node {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::node::sortNodesByDistance;
    using dht_hunter::utility::node::findNodeByEndpoint;
    using dht_hunter::utility::node::findNodeByID;
    using dht_hunter::utility::node::generateNodeLookupID;
    using dht_hunter::utility::node::generatePeerLookupID;
}

// Forward declarations for metadata utilities
namespace dht_hunter::utility::metadata {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::metadata::setInfoHashName;
    using dht_hunter::utility::metadata::getInfoHashName;
    using dht_hunter::utility::metadata::addFileToInfoHash;
    using dht_hunter::utility::metadata::getInfoHashFiles;
    using dht_hunter::utility::metadata::getInfoHashTotalSize;
    using dht_hunter::utility::metadata::getInfoHashMetadata;
    using dht_hunter::utility::metadata::getAllMetadata;
}

// Forward declarations for thread utilities
namespace dht_hunter::utility::thread {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::system::thread::LockTimeoutException;
    using dht_hunter::utility::system::thread::g_shuttingDown;
    using dht_hunter::utility::system::thread::withLock;
    using dht_hunter::utility::system::thread::runAsync;
    using dht_hunter::utility::system::thread::sleep;
    using dht_hunter::utility::system::thread::ThreadPool;
}

// Forward declarations for process utilities
namespace dht_hunter::utility::process {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::system::process::getMemoryUsage;
    using dht_hunter::utility::system::process::formatSize;
}

// Forward declarations for memory utilities
namespace dht_hunter::utility::system {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::system::memory::getTotalSystemMemory;
    using dht_hunter::utility::system::memory::getAvailableSystemMemory;
    using dht_hunter::utility::system::memory::calculateMaxTransactions;
}

// Forward declarations for random utilities
namespace dht_hunter::utility::random {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::random::generateRandomBytes;
    using dht_hunter::utility::random::generateRandomHexString;
    using dht_hunter::utility::random::generateTransactionID;
    using dht_hunter::utility::random::generateToken;
    using dht_hunter::utility::random::RandomGenerator;
}

// Forward declarations for JSON utilities
namespace dht_hunter::utility::json {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::json::JsonValue;
    using dht_hunter::utility::json::Json;
}

// Forward declarations for configuration utilities
namespace dht_hunter::utility::config {
    // Forward to the new consolidated utilities
    using dht_hunter::utility::config::ConfigurationManager;
}
