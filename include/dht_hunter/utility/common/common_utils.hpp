#pragma once

/**
 * @file common_utils.hpp
 * @brief Legacy common utility functions and types for the DHT Hunter project
 *
 * This file is maintained for backward compatibility and forwards to the new
 * consolidated utility modules.
 */

// Include consolidated utilities
#include "utils/common_utils.hpp"

namespace dht_hunter::utility::common {

// Import the Result class from the consolidated utilities
using dht_hunter::utility::common::Result;

// Use the constants from the consolidated utilities
// The constants are already in the same namespace

} // namespace dht_hunter::utility::common
