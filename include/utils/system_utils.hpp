#pragma once

/**
 * @file system_utils.hpp
 * @brief System utility functions and types for the BitScrape project
 * 
 * This file consolidates system-level functionality from various utility files:
 * - thread_utils.hpp (Thread management utilities)
 * - thread_pool.hpp (Thread pool implementation)
 * - process_utils.hpp (Process management utilities)
 * - memory_utils.hpp (Memory management utilities)
 */

// Standard C++ libraries
#include <mutex>
#include <chrono>
#include <string>
#include <functional>
#include <stdexcept>
#include <thread>
#include <future>
#include <vector>
#include <queue>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace dht_hunter::utility {

//-----------------------------------------------------------------------------
// System utilities
//-----------------------------------------------------------------------------
namespace system {

    //-------------------------------------------------------------------------
    // Thread utilities
    //-------------------------------------------------------------------------
    namespace thread {

        /**
         * @brief Exception thrown when a lock cannot be acquired
         */
        class LockTimeoutException : public std::runtime_error {
        public:
            explicit LockTimeoutException(const std::string& message)
                : std::runtime_error(message) {}
        };

        // Global flag to indicate if the program is shutting down
        extern std::atomic<bool> g_shuttingDown;

        /**
         * @brief Executes a function with a lock, handling timeouts and exceptions
         *
         * @param mutex The mutex to lock
         * @param func The function to execute while the lock is held
         * @param lockName The name of the lock (for debugging)
         * @param timeoutMs The timeout in milliseconds (default: 10000)
         * @param maxRetries The maximum number of retries (default: 20)
         * @return The result of the function
         * @throws LockTimeoutException if the lock cannot be acquired after all retries
         * @throws Any exception thrown by the function
         */
        template <typename Mutex, typename Func>
        auto withLock(Mutex& mutex, Func func, const std::string& lockName = "unnamed",
                    unsigned int timeoutMs = 10000, unsigned int maxRetries = 20)
            -> decltype(func()) {

            // If we're shutting down, don't try to acquire locks
            if (g_shuttingDown.load(std::memory_order_acquire)) {
                // Return a default-constructed value if not void
                if constexpr (std::is_void_v<decltype(func())>) {
                    return;
                } else {
                    return decltype(func()){};
                }
            }

            try {
                // Use the appropriate lock guard based on the mutex type
                if constexpr (std::is_same_v<Mutex, std::recursive_mutex>) {
                    // For recursive mutexes, use try_lock with timeout
                    for (unsigned int attempt = 0; attempt <= maxRetries; ++attempt) {
                        // Check if we're shutting down
                        if (g_shuttingDown.load(std::memory_order_acquire)) {
                            // Return a default-constructed value if not void
                            if constexpr (std::is_void_v<decltype(func())>) {
                                return;
                            } else {
                                return decltype(func()){};
                            }
                        }

                        if (mutex.try_lock()) {
                            std::lock_guard<std::recursive_mutex> guard(mutex, std::adopt_lock);
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
                        // Check if we're shutting down
                        if (g_shuttingDown.load(std::memory_order_acquire)) {
                            // Return a default-constructed value if not void
                            if constexpr (std::is_void_v<decltype(func())>) {
                                return;
                            } else {
                                return decltype(func()){};
                            }
                        }

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
                }
            } catch (const LockTimeoutException&) {
                // Re-throw lock timeout exceptions
                throw;
            } catch (const std::exception& e) {
                // Wrap other exceptions with lock information
                throw std::runtime_error("Exception while executing function with lock '" + lockName + "': " + e.what());
            } catch (...) {
                // Wrap unknown exceptions with lock information
                throw std::runtime_error("Unknown exception while executing function with lock '" + lockName + "'");
            }

            // This should never be reached, but return a default value to satisfy the compiler
            if constexpr (std::is_void_v<decltype(func())>) {
                return;
            } else {
                return decltype(func()){};
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

        /**
         * @brief A simple thread pool implementation
         */
        class ThreadPool {
        public:
            /**
             * @brief Constructor
             * @param numThreads The number of worker threads to create
             */
            ThreadPool(size_t numThreads);

            /**
             * @brief Destructor
             *
             * Stops all threads and joins them
             */
            ~ThreadPool();

            /**
             * @brief Enqueues a task to be executed by the thread pool
             *
             * @tparam F The type of the function to execute
             * @tparam Args The types of the arguments to pass to the function
             * @param f The function to execute
             * @param args The arguments to pass to the function
             * @return A future containing the result of the function
             */
            template<class F, class... Args>
            auto enqueue(F&& f, Args&&... args)
                -> std::future<typename std::invoke_result<F, Args...>::type>;

        private:
            // Worker threads
            std::vector<std::thread> m_workers;
            
            // Task queue
            std::queue<std::function<void()>> m_tasks;
            
            // Synchronization
            std::mutex m_queueMutex;
            std::condition_variable m_condition;
            bool m_stop;
        };

        // Implementation of the enqueue method
        template<class F, class... Args>
        auto ThreadPool::enqueue(F&& f, Args&&... args)
            -> std::future<typename std::invoke_result<F, Args...>::type> {

            using return_type = typename std::invoke_result<F, Args...>::type;

            // Create a packaged task that will execute the function
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            // Get the future result
            std::future<return_type> result = task->get_future();

            // Add the task to the queue
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);

                // Don't allow enqueueing after stopping the pool
                if (m_stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                // Add the task to the queue
                m_tasks.emplace([task]() { (*task)(); });
            }

            // Notify a worker thread
            m_condition.notify_one();
            
            return result;
        }

    } // namespace thread

    //-------------------------------------------------------------------------
    // Process utilities
    //-------------------------------------------------------------------------
    namespace process {

        /**
         * @brief Gets the current process memory usage in bytes
         * @return The memory usage in bytes
         */
        uint64_t getMemoryUsage();

        /**
         * @brief Formats a size in bytes to a human-readable string
         * @param bytes The size in bytes
         * @return A human-readable string (e.g., "1.23 MB")
         */
        std::string formatSize(uint64_t bytes);

    } // namespace process

    //-------------------------------------------------------------------------
    // Memory utilities
    //-------------------------------------------------------------------------
    namespace memory {

        /**
         * @brief Gets the total system memory in bytes
         * @return The total system memory in bytes, or 0 if it couldn't be determined
         */
        uint64_t getTotalSystemMemory();

        /**
         * @brief Gets the available system memory in bytes
         * @return The available system memory in bytes, or 0 if it couldn't be determined
         */
        uint64_t getAvailableSystemMemory();

        /**
         * @brief Calculates the maximum number of transactions based on available memory
         * @param percentageOfMemory The percentage of available memory to use (0.0-1.0)
         * @param bytesPerTransaction The estimated bytes per transaction
         * @param minTransactions The minimum number of transactions to allow
         * @param maxTransactions The maximum number of transactions to allow
         * @return The calculated maximum number of transactions
         */
        size_t calculateMaxTransactions(
            double percentageOfMemory = 0.25,
            size_t bytesPerTransaction = 350,
            size_t minTransactions = 1000,
            size_t maxTransactions = 1000000);

    } // namespace memory

} // namespace system

} // namespace dht_hunter::utility
