#pragma once

#include <mutex>
#include <chrono>
#include <string>
#include <functional>
#include <stdexcept>
#include <thread>
#include <future>

namespace dht_hunter::utility::thread {

/**
 * @brief Exception thrown when a lock cannot be acquired
 */
class LockTimeoutException : public std::runtime_error {
public:
    explicit LockTimeoutException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Executes a function with a lock, handling timeouts and exceptions
 *
 * @param mutex The mutex to lock
 * @param func The function to execute while the lock is held
 * @param lockName The name of the lock (for debugging)
 * @param timeoutMs The timeout in milliseconds (default: 5000)
 * @param maxRetries The maximum number of retries (default: 10)
 * @return The result of the function
 * @throws LockTimeoutException if the lock cannot be acquired after all retries
 * @throws Any exception thrown by the function
 */
template <typename Mutex, typename Func>
auto withLock(Mutex& mutex, Func func, const std::string& lockName = "unnamed",
             unsigned int timeoutMs = 5000, unsigned int maxRetries = 10)
    -> decltype(func()) {
    // Use the appropriate lock guard based on the mutex type
    if constexpr (std::is_same_v<Mutex, std::recursive_mutex>) {
        // For recursive mutexes, use std::lock_guard directly
        std::lock_guard<std::recursive_mutex> guard(mutex);
        return func();
    } else if constexpr (std::is_same_v<Mutex, std::timed_mutex>) {
        // For timed mutexes, try to lock with timeout
        if (mutex.try_lock_for(std::chrono::milliseconds(timeoutMs))) {
            std::lock_guard<std::timed_mutex> guard(mutex, std::adopt_lock);
            return func();
        } else {
            throw LockTimeoutException("Failed to acquire lock '" + lockName + "' after " +
                                     std::to_string(timeoutMs) + " ms");
        }
    } else {
        // For regular mutexes, try to lock with retries
        for (unsigned int attempt = 0; attempt <= maxRetries; ++attempt) {
            if (mutex.try_lock()) {
                std::lock_guard<std::mutex> guard(mutex, std::adopt_lock);
                return func();
            }

            // If this was the last attempt, throw an exception
            if (attempt == maxRetries) {
                throw LockTimeoutException("Failed to acquire lock '" + lockName + "' after " +
                                         std::to_string(maxRetries + 1) + " attempts");
            }

            // Sleep before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs / (maxRetries + 1)));
        }

        // This should never be reached, but just in case
        throw LockTimeoutException("Failed to acquire lock '" + lockName + "'");
    }
}

/**
 * @brief Executes a function asynchronously
 *
 * @param func The function to execute
 * @param args The arguments to pass to the function
 * @return A future containing the result of the function
 */
template <typename Func, typename... Args>
auto runAsync(Func&& func, Args&&... args) {
    return std::async(std::launch::async, std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * @brief Sleeps for the specified duration
 *
 * @param milliseconds The duration to sleep in milliseconds
 */
inline void sleep(unsigned int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace dht_hunter::utility::thread
