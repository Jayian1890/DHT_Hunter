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
 * @brief Exception thrown when a lock timeout occurs
 */
class LockTimeoutException : public std::runtime_error {
public:
    explicit LockTimeoutException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief RAII wrapper for a mutex with timeout
 * 
 * This class is similar to std::lock_guard but with a timeout to prevent deadlocks.
 */
class SafeLockGuard {
public:
    /**
     * @brief Constructor that acquires the lock with a timeout
     * 
     * @param mutex The mutex to lock
     * @param lockName The name of the lock (for debugging)
     * @throws LockTimeoutException if the lock cannot be acquired
     */
    SafeLockGuard(std::mutex& mutex, const std::string& lockName = "unnamed")
        : m_mutex(mutex), m_locked(false) {
        if (!mutex.try_lock()) {
            throw LockTimeoutException("Failed to acquire lock '" + lockName + "'");
        }
        m_locked = true;
    }

    /**
     * @brief Destructor that releases the lock
     */
    ~SafeLockGuard() {
        if (m_locked) {
            m_mutex.unlock();
        }
    }

    /**
     * @brief Delete copy constructor and assignment operator
     */
    SafeLockGuard(const SafeLockGuard&) = delete;
    SafeLockGuard& operator=(const SafeLockGuard&) = delete;

private:
    std::mutex& m_mutex;
    bool m_locked;
};

/**
 * @brief Executes a function with a lock, handling timeouts and exceptions
 * 
 * @param mutex The mutex to lock
 * @param func The function to execute while the lock is held
 * @param lockName The name of the lock (for debugging)
 * @return The result of the function
 * @throws LockTimeoutException if the lock cannot be acquired
 * @throws Any exception thrown by the function
 */
template <typename Func>
auto withLock(std::mutex& mutex, Func func, const std::string& lockName = "unnamed") 
    -> decltype(func()) {
    SafeLockGuard guard(mutex, lockName);
    return func();
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
