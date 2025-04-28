#pragma once

// This file is a transitional adapter to help migrate from the old DHT types
// to the new Types module. It provides the same types and functions as the
// old DHT types, but using the new Types module.

#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/dht_types.hpp"
#include "dht_hunter/types/endpoint.hpp"

namespace dht_hunter::dht {

// Import types from the Types module
using NodeID = types::NodeID;
using InfoHash = types::InfoHash;
using Node = types::Node;
using TransactionID = types::TransactionID;

// Import functions from the Types module
using types::calculateDistance;
using types::nodeIDToString;
using types::infoHashToString;
using types::infoHashFromString;
using types::generateRandomNodeID;
using types::createEmptyInfoHash;
using types::isValidNodeID;
using types::isValidInfoHash;

// Import network types
using types::EndPoint;

} // namespace dht_hunter::dht
