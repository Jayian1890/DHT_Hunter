#include "dht_hunter/utility/thread/thread_pool.hpp"

namespace dht_hunter::utility::thread {

ThreadPool::ThreadPool(size_t numThreads) : m_stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    
                    // Wait for a task or stop signal
                    m_condition.wait(lock, [this] {
                        return m_stop || !m_tasks.empty();
                    });
                    
                    // Exit if stopped and no tasks
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    
                    // Get the next task
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                
                // Execute the task
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    
    // Notify all threads to check the stop flag
    m_condition.notify_all();
    
    // Join all threads
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace dht_hunter::utility::thread
