#pragma once

#include "dht_hunter/types/event_types.hpp"

namespace dht_hunter::unified_event {

// Use the event types from the Types module
using EventSeverity = types::EventSeverity;
using EventType = types::EventType;

// Import functions from the Types module
using types::eventTypeToString;
using types::eventSeverityToString;

} // namespace dht_hunter::unified_event
