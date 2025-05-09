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
