#pragma once

#include <future>
#include <functional>
#include <string>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <iostream>

namespace utils {

/**
 * @brief Executes a function asynchronously
 * @tparam F The function type
 * @tparam Args The argument types
 * @param func The function to execute
 * @param args The function arguments
 * @return A future for the function result
 */
template<typename F, typename... Args>
auto executeAsync(F&& func, Args&&... args) {
    return std::async(std::launch::async, std::forward<F>(func), std::forward<Args>(args)...);
}

/**
 * @brief Executes a function asynchronously with a timeout
 * @tparam R The return type of the function
 * @tparam F The function type
 * @tparam Args The argument types
 * @param timeout The timeout duration
 * @param defaultValue The default value to return if the timeout expires
 * @param func The function to execute
 * @param args The function arguments
 * @return The function result, or the default value if the timeout expires
 */
template<typename R, typename F, typename... Args>
R executeAsyncWithTimeout(std::chrono::milliseconds timeout, R defaultValue, F&& func, Args&&... args) {
    auto future = std::async(std::launch::async, std::forward<F>(func), std::forward<Args>(args)...);
    
    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    } else {
        return defaultValue;
    }
}

/**
 * @brief Executes a function asynchronously with a timeout and callback
 * @tparam R The return type of the function
 * @tparam F The function type
 * @tparam C The callback type
 * @tparam Args The argument types
 * @param timeout The timeout duration
 * @param func The function to execute
 * @param callback The callback to call with the result
 * @param args The function arguments
 */
template<typename R, typename F, typename C, typename... Args>
void executeAsyncWithCallback(std::chrono::milliseconds timeout, F&& func, C&& callback, Args&&... args) {
    auto future = std::async(std::launch::async, [func = std::forward<F>(func), args = std::make_tuple(std::forward<Args>(args)...)]() {
        try {
            return std::apply(func, args);
        } catch (const std::exception& e) {
            std::cerr << "Exception in async task: " << e.what() << std::endl;
            throw;
        } catch (...) {
            std::cerr << "Unknown exception in async task" << std::endl;
            throw;
        }
    });
    
    // Create another async task to wait for the result and call the callback
    std::async(std::launch::async, [future = std::move(future), timeout, callback = std::forward<C>(callback)]() mutable {
        try {
            if (future.wait_for(timeout) == std::future_status::ready) {
                callback(future.get(), true);
            } else {
                callback(R(), false);
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception waiting for async result: " << e.what() << std::endl;
            callback(R(), false);
        } catch (...) {
            std::cerr << "Unknown exception waiting for async result" << std::endl;
            callback(R(), false);
        }
    });
}

/**
 * @brief Executes a function periodically
 * @tparam F The function type
 * @tparam Args The argument types
 * @param interval The interval between executions
 * @param func The function to execute
 * @param args The function arguments
 * @return A future that will be fulfilled when the periodic execution is stopped
 */
template<typename F, typename... Args>
std::future<void> executePeriodicAsync(std::chrono::milliseconds interval, F&& func, Args&&... args) {
    // Create a shared state to control the periodic execution
    auto running = std::make_shared<std::atomic<bool>>(true);
    
    // Create a promise to signal when the periodic execution is stopped
    auto promise = std::make_shared<std::promise<void>>();
    
    // Start the periodic execution
    std::async(std::launch::async, [running, promise, interval, func = std::forward<F>(func), args = std::make_tuple(std::forward<Args>(args)...)]() {
        try {
            while (running->load()) {
                std::apply(func, args);
                std::this_thread::sleep_for(interval);
            }
            promise->set_value();
        } catch (const std::exception& e) {
            std::cerr << "Exception in periodic task: " << e.what() << std::endl;
            try {
                promise->set_exception(std::current_exception());
            } catch (...) {
                // Promise might already be satisfied
            }
        } catch (...) {
            std::cerr << "Unknown exception in periodic task" << std::endl;
            try {
                promise->set_exception(std::current_exception());
            } catch (...) {
                // Promise might already be satisfied
            }
        }
    });
    
    // Return a future that will stop the periodic execution when destroyed
    return std::async(std::launch::deferred, [running, promise]() {
        running->store(false);
        return promise->get_future().get();
    });
}

} // namespace utils
