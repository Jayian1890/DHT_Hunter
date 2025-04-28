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
 * @brief Base class for SafeLockGuard implementations
 */
class SafeLockGuardBase {
protected:
    bool m_locked;

    SafeLockGuardBase() : m_locked(false) {}
    virtual ~SafeLockGuardBase() {}
};

/**
 * @brief RAII wrapper for a mutex with timeout
 *
 * This class is similar to std::lock_guard but with a timeout to prevent deadlocks.
 * Specialized for std::mutex.
 */
class SafeLockGuard : public SafeLockGuardBase {
public:
    /**
     * @brief Constructor that acquires the lock with a timeout
     *
     * @param mutex The mutex to lock
     * @param lockName The name of the lock (for debugging)
     * @param timeoutMs The timeout in milliseconds (default: 5000)
     * @param maxRetries The maximum number of retries (default: 10)
     * @throws LockTimeoutException if the lock cannot be acquired after all retries
     */
    SafeLockGuard(std::mutex& mutex, const std::string& lockName = "unnamed",
                 unsigned int timeoutMs = 5000, unsigned int maxRetries = 10)
        : m_mutex(&mutex) {

        // Try to acquire the lock with retries
        for (unsigned int attempt = 0; attempt <= maxRetries; ++attempt) {
            // Try to acquire the lock
            if (mutex.try_lock()) {
                m_locked = true;
                return;
            }

            // If this was the last attempt, throw an exception
            if (attempt == maxRetries) {
                throw LockTimeoutException("Failed to acquire lock '" + lockName + "' after " +
                                         std::to_string(maxRetries + 1) + " attempts");
            }

            // Sleep before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs / (maxRetries + 1)));
        }
    }

    /**
     * @brief Destructor that releases the lock
     */
    ~SafeLockGuard() {
        if (m_locked && m_mutex) {
            m_mutex->unlock();
        }
    }

    /**
     * @brief Delete copy constructor and assignment operator
     */
    SafeLockGuard(const SafeLockGuard&) = delete;
    SafeLockGuard& operator=(const SafeLockGuard&) = delete;

private:
    std::mutex* m_mutex;
};

/**
 * @brief RAII wrapper for a timed mutex with timeout
 *
 * This class is similar to std::lock_guard but with a timeout to prevent deadlocks.
 * Specialized for std::timed_mutex.
 */
class TimedSafeLockGuard : public SafeLockGuardBase {
public:
    /**
     * @brief Constructor that acquires the lock with a timeout
     *
     * @param mutex The mutex to lock
     * @param lockName The name of the lock (for debugging)
     * @param timeoutMs The timeout in milliseconds (default: 5000)
     * @throws LockTimeoutException if the lock cannot be acquired
     */
    TimedSafeLockGuard(std::timed_mutex& mutex, const std::string& lockName = "unnamed",
                      unsigned int timeoutMs = 5000)
        : m_mutex(&mutex) {

        // Try to acquire the lock with timeout
        if (mutex.try_lock_for(std::chrono::milliseconds(timeoutMs))) {
            m_locked = true;
        } else {
            throw LockTimeoutException("Failed to acquire lock '" + lockName + "' after " +
                                     std::to_string(timeoutMs) + " ms");
        }
    }

    /**
     * @brief Destructor that releases the lock
     */
    ~TimedSafeLockGuard() {
        if (m_locked && m_mutex) {
            m_mutex->unlock();
        }
    }

    /**
     * @brief Delete copy constructor and assignment operator
     */
    TimedSafeLockGuard(const TimedSafeLockGuard&) = delete;
    TimedSafeLockGuard& operator=(const TimedSafeLockGuard&) = delete;

private:
    std::timed_mutex* m_mutex;
};

/**
 * @brief RAII wrapper for a recursive mutex
 *
 * This class is similar to std::lock_guard but specialized for recursive mutexes.
 */
class RecursiveSafeLockGuard : public SafeLockGuardBase {
public:
    /**
     * @brief Constructor that acquires the lock
     *
     * @param mutex The mutex to lock
     * @param lockName The name of the lock (for debugging)
     */
    RecursiveSafeLockGuard(std::recursive_mutex& mutex, const std::string& lockName = "unnamed")
        : m_mutex(&mutex) {

        try {
            mutex.lock();
            m_locked = true;
        } catch (const std::system_error& e) {
            throw LockTimeoutException("Failed to acquire lock '" + lockName + "': " + e.what());
        }
    }

    /**
     * @brief Destructor that releases the lock
     */
    ~RecursiveSafeLockGuard() {
        if (m_locked && m_mutex) {
            m_mutex->unlock();
        }
    }

    /**
     * @brief Delete copy constructor and assignment operator
     */
    RecursiveSafeLockGuard(const RecursiveSafeLockGuard&) = delete;
    RecursiveSafeLockGuard& operator=(const RecursiveSafeLockGuard&) = delete;

private:
    std::recursive_mutex* m_mutex;
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
    if constexpr (std::is_same_v<Mutex, std::timed_mutex>) {
        TimedSafeLockGuard guard(mutex, lockName, timeoutMs);
        return func();
    } else if constexpr (std::is_same_v<Mutex, std::recursive_mutex>) {
        RecursiveSafeLockGuard guard(mutex, lockName);
        return func();
    } else {
        SafeLockGuard guard(mutex, lockName, timeoutMs, maxRetries);
        return func();
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
