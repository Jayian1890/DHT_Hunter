#include "dht_hunter/utility/thread/thread_utils.hpp"

namespace dht_hunter::utility::thread {

// Initialize the global shutdown flag
std::atomic<bool> g_shuttingDown(false);

} // namespace dht_hunter::utility::thread
