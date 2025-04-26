#pragma once

#include <cstddef>
#include <cstdint>

namespace dht_hunter::dht {

/**
 * @brief Default port for DHT nodes
 */
constexpr uint16_t DEFAULT_PORT = 6881;

/**
 * @brief Maximum number of concurrent transactions
 */
constexpr size_t MAX_TRANSACTIONS = 1024;

/**
 * @brief Transaction timeout in seconds
 */
constexpr int TRANSACTION_TIMEOUT = 30;

/**
 * @brief Default alpha parameter for parallel lookups (number of concurrent requests)
 */
constexpr size_t DEFAULT_LOOKUP_ALPHA = 3;

/**
 * @brief Default maximum number of nodes to store in a lookup result
 */
constexpr size_t DEFAULT_LOOKUP_MAX_RESULTS = 8;

/**
 * @brief Maximum number of iterations for a node lookup
 */
constexpr size_t LOOKUP_MAX_ITERATIONS = 20;

/**
 * @brief Maximum number of peers to store per info hash
 */
constexpr size_t MAX_PEERS_PER_INFOHASH = 100;

/**
 * @brief Time-to-live for stored peers in seconds (30 minutes)
 */
constexpr int PEER_TTL = 1800;

/**
 * @brief Interval for cleaning up expired peers in seconds (5 minutes)
 */
constexpr int PEER_CLEANUP_INTERVAL = 300;

/**
 * @brief Interval for saving the routing table in seconds (10 minutes)
 */
constexpr int ROUTING_TABLE_SAVE_INTERVAL = 600;

/**
 * @brief Default k-bucket size (maximum number of nodes in a bucket)
 */
constexpr size_t K_BUCKET_SIZE = 8;

/**
 * @brief Default MTU size for UDP packets
 */
constexpr size_t DEFAULT_MTU_SIZE = 1400;

/**
 * @brief Default bootstrap nodes
 */
constexpr const char* DEFAULT_BOOTSTRAP_NODES[] = {
    "router.bittorrent.com:6881",
    "dht.transmissionbt.com:6881",
    "router.utorrent.com:6881"
};

/**
 * @brief Number of default bootstrap nodes
 */
constexpr size_t DEFAULT_BOOTSTRAP_NODES_COUNT = 3;

/**
 * @brief Default bootstrap timeout in seconds
 */
constexpr int DEFAULT_BOOTSTRAP_TIMEOUT = 10;

/**
 * @brief Default token rotation interval in seconds
 */
constexpr int DEFAULT_TOKEN_ROTATION_INTERVAL = 600;

} // namespace dht_hunter::dht
