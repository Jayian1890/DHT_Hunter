#pragma once

#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace dht_hunter {
namespace utility {
namespace thread {

// Thread pool class
class ThreadPool {
public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
        : stop_(false) {
        // Placeholder for actual implementation
    }

    ~ThreadPool() {
        // Placeholder for actual implementation
    }

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        // Placeholder for actual implementation
        using return_type = typename std::invoke_result<F, Args...>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();

        // Execute the task
        (*task)();

        return res;
    }

private:
    std::vector<std::thread> workers_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// Utility functions
inline void setThreadName(const std::string& name) {
    // Placeholder for actual implementation
}

inline std::string getThreadName() {
    // Placeholder for actual implementation
    return "thread";
}

// Lock timeout exception
class LockTimeoutException : public std::runtime_error {
public:
    LockTimeoutException(const std::string& message)
        : std::runtime_error(message) {}
};

// Thread locking utility
template<typename T, typename F>
auto withLock(T& mutex, F&& func) -> decltype(func()) {
    std::lock_guard<T> lock(mutex);
    return func();
}

// Namespace alias for backward compatibility
namespace dht_hunter {
namespace utility {
namespace thread {
    using ::dht_hunter::utility::thread::ThreadPool;
    using ::dht_hunter::utility::thread::LockTimeoutException;

    // This is a different function signature to support legacy code
    template<typename T, typename F>
    auto withLock(T& mutex, F&& func) -> decltype(func()) {
        std::lock_guard<T> lock(mutex);
        return func();
    }

    // Legacy code uses this signature
    template<typename T, typename F>
    auto withLock(const T&, T& mutex, F&& func) -> decltype(func()) {
        std::lock_guard<T> lock(mutex);
        return func();
    }

    

    inline void setThreadName(const std::string& name) {
        ::dht_hunter::utility::thread::setThreadName(name);
    }

    inline std::string getThreadName() {
        return ::dht_hunter::utility::thread::getThreadName();
    }
} // namespace thread
} // namespace utility
} // namespace dht_hunter

} // namespace thread
} // namespace utility
} // namespace dht_hunter
