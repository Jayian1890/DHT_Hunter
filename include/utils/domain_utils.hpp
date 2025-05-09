#pragma once

/**
 * @file domain_utils.hpp
 * @brief Domain-specific utility functions and types for the BitScrape project
 * 
 * This file consolidates domain-specific functionality from various utility files:
 * - network_utils.hpp (Network-related utilities)
 * - node_utils.hpp (Node-related utilities)
 * - metadata_utils.hpp (Metadata-related utilities)
 */

// Standard C++ libraries
#include <string>
#include <vector>
#include <memory>
#include <chrono>

// Project includes
#include "utils/common_utils.hpp"
#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/types/dht_types.hpp"

namespace dht_hunter::utility {

//-----------------------------------------------------------------------------
// Network utilities
//-----------------------------------------------------------------------------
namespace network {

/**
 * @brief Gets the user agent string from the configuration
 * @return The user agent string
 */
std::string getUserAgent();

} // namespace network

//-----------------------------------------------------------------------------
// Node utilities
//-----------------------------------------------------------------------------
namespace node {

/**
 * @brief Sorts nodes by distance to a target
 * @param nodes The nodes to sort
 * @param targetID The target ID
 * @return The sorted nodes
 */
std::vector<std::shared_ptr<types::Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& targetID);

/**
 * @brief Finds a node by endpoint
 * @param nodes The nodes to search
 * @param endpoint The endpoint to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<types::Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::EndPoint& endpoint);

/**
 * @brief Finds a node by ID
 * @param nodes The nodes to search
 * @param nodeID The node ID to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<types::Node> findNodeByID(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& nodeID);

/**
 * @brief Generates a lookup ID from a node ID
 * @param nodeID The node ID
 * @return The lookup ID
 */
std::string generateNodeLookupID(const types::NodeID& nodeID);

/**
 * @brief Generates a lookup ID from an info hash
 * @param infoHash The info hash
 * @return The lookup ID
 */
std::string generatePeerLookupID(const types::InfoHash& infoHash);

} // namespace node

//-----------------------------------------------------------------------------
// Metadata utilities
//-----------------------------------------------------------------------------
namespace metadata {

/**
 * @brief Sets the name for an info hash
 * @param infoHash The info hash
 * @param name The name
 * @return True if successful, false otherwise
 */
bool setInfoHashName(const types::InfoHash& infoHash, const std::string& name);

/**
 * @brief Gets the name for an info hash
 * @param infoHash The info hash
 * @return The name, or empty string if not found
 */
std::string getInfoHashName(const types::InfoHash& infoHash);

/**
 * @brief Adds a file to an info hash
 * @param infoHash The info hash
 * @param path The file path
 * @param size The file size
 * @return True if successful, false otherwise
 */
bool addFileToInfoHash(const types::InfoHash& infoHash, const std::string& path, uint64_t size);

/**
 * @brief Gets the files for an info hash
 * @param infoHash The info hash
 * @return The files, or empty vector if not found
 */
std::vector<types::TorrentFile> getInfoHashFiles(const types::InfoHash& infoHash);

/**
 * @brief Gets the total size of all files for an info hash
 * @param infoHash The info hash
 * @return The total size in bytes, or 0 if not found
 */
uint64_t getInfoHashTotalSize(const types::InfoHash& infoHash);

/**
 * @brief Gets the metadata for an info hash
 * @param infoHash The info hash
 * @return The metadata, or nullptr if not found
 */
std::shared_ptr<types::InfoHashMetadata> getInfoHashMetadata(const types::InfoHash& infoHash);

/**
 * @brief Gets all metadata
 * @return All metadata
 */
std::vector<std::shared_ptr<types::InfoHashMetadata>> getAllMetadata();

} // namespace metadata

} // namespace dht_hunter::utility
