#pragma once

/**
 * @file memory_utils.hpp
 * @brief Legacy memory utilities header that forwards to the new consolidated utilities
 *
 * This file provides backward compatibility for legacy code that still includes
 * the old memory utilities header. It forwards to the new consolidated utilities.
 *
 * @deprecated Use utils/system_utils.hpp instead
 */

#include "dht_hunter/utility/legacy_utils.hpp"

// The functions are now provided by the legacy_utils.hpp file
// which includes the system_utils.hpp file and sets up the namespace aliases

