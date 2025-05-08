#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace dht_hunter::utility::thread {

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

    /**
     * @brief Get the number of threads in the pool
     * @return The number of threads
     */
    size_t getThreadCount() const {
        return m_workers.size();
    }

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

} // namespace dht_hunter::utility::thread
