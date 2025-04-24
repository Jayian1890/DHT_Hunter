#pragma once

#include <mutex>
#include <thread>

namespace dht_hunter::util {

/**
 * @brief Utility class to check if the current thread owns a mutex
 *
 * This class is used to prevent recursive locking of a mutex, which can lead to deadlocks.
 * It's a debugging tool that helps identify potential deadlock situations.
 */
class MutexOwnershipChecker {
public:
    /**
     * @brief Constructor
     */
    MutexOwnershipChecker() : m_ownerThreadId(std::thread::id()), m_locked(false) {}

    /**
     * @brief Mark the mutex as locked by the current thread
     */
    void lock() {
        m_ownerThreadId = std::this_thread::get_id();
        m_locked = true;
    }

    /**
     * @brief Mark the mutex as unlocked
     */
    void unlock() {
        m_locked = false;
        m_ownerThreadId = std::thread::id();
    }

    /**
     * @brief Check if the mutex is locked by the current thread
     * @return True if the mutex is locked by the current thread, false otherwise
     */
    bool isLockedByCurrentThread() const {
        return m_locked && m_ownerThreadId == std::this_thread::get_id();
    }

private:
    std::thread::id m_ownerThreadId;
    bool m_locked;
};

/**
 * @brief A mutex wrapper that checks for recursive locking
 *
 * This class wraps a std::mutex and adds checking to prevent recursive locking.
 * It's useful for debugging and preventing deadlocks.
 */
class CheckedMutex {
public:
    /**
     * @brief Lock the mutex
     * @throws std::runtime_error if the mutex is already locked by the current thread
     */
    void lock() {
        if (m_checker.isLockedByCurrentThread()) {
            throw std::runtime_error("Recursive mutex lock detected");
        }
        m_mutex.lock();
        m_checker.lock();
    }

    /**
     * @brief Try to lock the mutex
     * @return True if the mutex was locked, false otherwise
     * @throws std::runtime_error if the mutex is already locked by the current thread
     */
    bool try_lock() {
        if (m_checker.isLockedByCurrentThread()) {
            throw std::runtime_error("Recursive mutex lock detected");
        }
        bool locked = m_mutex.try_lock();
        if (locked) {
            m_checker.lock();
        }
        return locked;
    }

    /**
     * @brief Unlock the mutex
     */
    void unlock() {
        m_checker.unlock();
        m_mutex.unlock();
    }

    /**
     * @brief Check if the mutex is locked by the current thread
     * @return True if the mutex is locked by the current thread, false otherwise
     */
    bool isLockedByCurrentThread() const {
        return m_checker.isLockedByCurrentThread();
    }

    /**
     * @brief Get the underlying std::mutex
     * @return Reference to the underlying std::mutex
     * @note This is needed for compatibility with std::lock_guard
     */
    operator std::mutex&() {
        return m_mutex;
    }

private:
    std::mutex m_mutex;
    MutexOwnershipChecker m_checker;
};

} // namespace dht_hunter::util
