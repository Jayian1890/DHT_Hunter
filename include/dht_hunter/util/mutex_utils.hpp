#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>

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
 * @brief A recursive mutex wrapper with debugging capabilities
 *
 * This class wraps a std::recursive_mutex and adds tracking to help with debugging.
 * It allows recursive locking (same thread can lock multiple times) which helps
 * prevent deadlocks in complex callback scenarios.
 */
class CheckedMutex {
public:
    /**
     * @brief Constructor
     */
    CheckedMutex() : m_lockCount(0) {}

    /**
     * @brief Lock the mutex
     *
     * This method can be called multiple times by the same thread.
     * Each call to lock() must be matched by a call to unlock().
     */
    void lock() {
        m_mutex.lock();
        if (m_checker.isLockedByCurrentThread()) {
            // This is a recursive lock - just increment the count
            m_lockCount++;
            // Uncomment for debugging
            // std::cerr << "Recursive lock detected, count: " << m_lockCount << std::endl;
        } else {
            // First lock by this thread
            m_checker.lock();
            m_lockCount = 1;
        }
    }

    /**
     * @brief Try to lock the mutex
     * @return True if the mutex was locked, false otherwise
     */
    bool try_lock() {
        bool locked = m_mutex.try_lock();
        if (locked) {
            if (m_checker.isLockedByCurrentThread()) {
                // This is a recursive lock - just increment the count
                m_lockCount++;
            } else {
                // First lock by this thread
                m_checker.lock();
                m_lockCount = 1;
            }
        }
        return locked;
    }

    /**
     * @brief Unlock the mutex
     */
    void unlock() {
        if (m_checker.isLockedByCurrentThread()) {
            m_lockCount--;
            if (m_lockCount == 0) {
                // Last unlock by this thread
                m_checker.unlock();
            }
        }
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
     * @brief Get the lock count for the current thread
     * @return The number of times the current thread has locked this mutex
     */
    int getLockCount() const {
        return m_checker.isLockedByCurrentThread() ? static_cast<int>(m_lockCount.load()) : 0;
    }

    /**
     * @brief Get the underlying std::recursive_mutex
     * @return Reference to the underlying std::recursive_mutex
     * @note This is needed for compatibility with std::lock_guard
     */
    operator std::mutex&() {
        return reinterpret_cast<std::mutex&>(m_mutex);
    }

private:
    std::recursive_mutex m_mutex;
    MutexOwnershipChecker m_checker;
    std::atomic<int> m_lockCount;
};

} // namespace dht_hunter::util
