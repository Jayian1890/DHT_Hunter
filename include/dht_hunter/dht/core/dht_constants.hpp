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
constexpr size_t DEFAULT_LOOKUP_ALPHA = 5;

/**
 * @brief Default maximum number of nodes to store in a lookup result
 */
constexpr size_t DEFAULT_LOOKUP_MAX_RESULTS = 16;

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
 * @brief Interval for saving transactions in seconds (5 minutes)
 */
constexpr int TRANSACTION_SAVE_INTERVAL = 300;

/**
 * @brief Default configuration directory
 */
constexpr char DEFAULT_CONFIG_DIR[] = "config";

/**
 * @brief Default setting for whether to save the routing table after each new node is added
 */
constexpr bool DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE = true;

/**
 * @brief Default setting for whether to save transactions on shutdown
 */
constexpr bool DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN = true;

/**
 * @brief Default setting for whether to load transactions on startup
 */
constexpr bool DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP = true;

/**
 * @brief Default path for the routing table file
 */
constexpr char DEFAULT_ROUTING_TABLE_PATH[] = "config/routing_table.dat";

/**
 * @brief Default maximum number of nodes in a k-bucket
 */
constexpr size_t DEFAULT_K_BUCKET_SIZE = 16;

/**
 * @brief Default maximum number of queued transactions
 */
constexpr size_t DEFAULT_MAX_QUEUED_TRANSACTIONS = 2048;

/**
 * @brief Default MTU size for UDP packets
 */
constexpr size_t DEFAULT_MTU_SIZE = 1400;

/**
 * @brief Default maximum number of messages per batch
 */
constexpr size_t DEFAULT_MAX_MESSAGES_PER_BATCH = 10;

/**
 * @brief Default maximum batch delay in milliseconds
 */
constexpr int DEFAULT_MAX_BATCH_DELAY_MS = 50;

/**
 * @brief Default thread join timeout in seconds
 */
constexpr int DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS = 2;

/**
 * @brief Default bootstrap timeout in seconds
 */
constexpr int DEFAULT_BOOTSTRAP_TIMEOUT_SECONDS = 5;

/**
 * @brief Default secret rotation interval in minutes
 */
constexpr int DEFAULT_SECRET_ROTATION_INTERVAL_MINUTES = 10;

/**
 * @brief Default secret length in characters
 */
constexpr size_t DEFAULT_SECRET_LENGTH = 20;

} // namespace dht_hunter::dht
