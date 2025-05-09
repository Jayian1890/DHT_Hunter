#pragma once

/**
 * @file node_lookup.hpp
 * @brief Backward compatibility header for node lookup components
 *
 * This file provides backward compatibility for code that includes the old
 * node_lookup.hpp file. It forwards to the new consolidated dht_core_utils.hpp file.
 */

#include "utils/dht_core_utils.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace dht_hunter::dht {

// Forward declarations
class TransactionManager;
class MessageSender;

// Use the consolidated NodeLookupState and NodeLookup classes from dht_core_utils.hpp

} // namespace dht_hunter::dht
