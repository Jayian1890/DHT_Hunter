#include "utils/system_utils.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cassert>

using namespace dht_hunter::utility;

/**
 * @brief Test the thread utilities
 * @return True if all tests pass, false otherwise
 */
bool testThreadUtils() {
    std::cout << "Testing thread utilities..." << std::endl;
    
    // Test withLock
    std::mutex testMutex;
    int testValue = 0;
    
    auto result = system::thread::withLock(testMutex, [&testValue]() {
        testValue = 42;
        return true;
    }, "test_mutex");
    
    if (!result) {
        std::cerr << "withLock failed to return the correct value" << std::endl;
        return false;
    }
    
    if (testValue != 42) {
        std::cerr << "withLock failed to execute the function" << std::endl;
        return false;
    }
    
    // Test runAsync
    std::atomic<bool> asyncDone(false);
    auto future = system::thread::runAsync([&asyncDone]() {
        system::thread::sleep(100);
        asyncDone.store(true);
        return 42;
    });
    
    auto futureResult = future.get();
    if (futureResult != 42) {
        std::cerr << "runAsync failed to return the correct value" << std::endl;
        return false;
    }
    
    if (!asyncDone.load()) {
        std::cerr << "runAsync failed to execute the function" << std::endl;
        return false;
    }
    
    // Test sleep
    auto start = std::chrono::steady_clock::now();
    system::thread::sleep(100);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (duration < 50) {
        std::cerr << "sleep did not sleep for the expected duration" << std::endl;
        std::cerr << "Expected at least 50ms, Got: " << duration << "ms" << std::endl;
        return false;
    }
    
    // Test ThreadPool
    system::thread::ThreadPool pool(4);
    std::atomic<int> counter(0);
    std::vector<std::future<int>> results;
    
    for (int i = 0; i < 8; ++i) {
        results.emplace_back(pool.enqueue([&counter, i]() {
            system::thread::sleep(50);
            counter.fetch_add(1);
            return i;
        }));
    }
    
    for (size_t i = 0; i < results.size(); ++i) {
        int value = results[i].get();
        if (value != static_cast<int>(i)) {
            std::cerr << "ThreadPool returned incorrect value" << std::endl;
            std::cerr << "Expected: " << i << ", Got: " << value << std::endl;
            return false;
        }
    }
    
    if (counter.load() != 8) {
        std::cerr << "ThreadPool did not execute all tasks" << std::endl;
        std::cerr << "Expected: 8, Got: " << counter.load() << std::endl;
        return false;
    }
    
    std::cout << "Thread utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the process utilities
 * @return True if all tests pass, false otherwise
 */
bool testProcessUtils() {
    std::cout << "Testing process utilities..." << std::endl;
    
    // Test getMemoryUsage
    uint64_t memoryUsage = system::process::getMemoryUsage();
    if (memoryUsage == 0) {
        std::cerr << "getMemoryUsage returned 0" << std::endl;
        return false;
    }
    
    std::cout << "Current memory usage: " << memoryUsage << " bytes" << std::endl;
    
    // Test formatSize
    std::string formattedSize = system::process::formatSize(1024);
    if (formattedSize != "1.00 KB") {
        std::cerr << "formatSize failed" << std::endl;
        std::cerr << "Expected: '1.00 KB', Got: '" << formattedSize << "'" << std::endl;
        return false;
    }
    
    formattedSize = system::process::formatSize(1024 * 1024);
    if (formattedSize != "1.00 MB") {
        std::cerr << "formatSize failed" << std::endl;
        std::cerr << "Expected: '1.00 MB', Got: '" << formattedSize << "'" << std::endl;
        return false;
    }
    
    formattedSize = system::process::formatSize(1024 * 1024 * 1024);
    if (formattedSize != "1.00 GB") {
        std::cerr << "formatSize failed" << std::endl;
        std::cerr << "Expected: '1.00 GB', Got: '" << formattedSize << "'" << std::endl;
        return false;
    }
    
    std::cout << "Process utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the memory utilities
 * @return True if all tests pass, false otherwise
 */
bool testMemoryUtils() {
    std::cout << "Testing memory utilities..." << std::endl;
    
    // Test getTotalSystemMemory
    uint64_t totalMemory = system::memory::getTotalSystemMemory();
    if (totalMemory == 0) {
        std::cerr << "getTotalSystemMemory returned 0" << std::endl;
        return false;
    }
    
    std::cout << "Total system memory: " << system::process::formatSize(totalMemory) << std::endl;
    
    // Test getAvailableSystemMemory
    uint64_t availableMemory = system::memory::getAvailableSystemMemory();
    if (availableMemory == 0) {
        std::cerr << "getAvailableSystemMemory returned 0" << std::endl;
        return false;
    }
    
    std::cout << "Available system memory: " << system::process::formatSize(availableMemory) << std::endl;
    
    // Test calculateMaxTransactions
    size_t maxTransactions = system::memory::calculateMaxTransactions(0.25, 350, 1000, 1000000);
    if (maxTransactions < 1000) {
        std::cerr << "calculateMaxTransactions returned less than the minimum" << std::endl;
        return false;
    }
    
    std::cout << "Calculated max transactions: " << maxTransactions << std::endl;
    
    // Test with different parameters
    size_t maxTransactions2 = system::memory::calculateMaxTransactions(0.5, 350, 1000, 1000000);
    if (maxTransactions2 < maxTransactions) {
        std::cerr << "calculateMaxTransactions with higher percentage returned fewer transactions" << std::endl;
        return false;
    }
    
    size_t maxTransactions3 = system::memory::calculateMaxTransactions(0.25, 700, 1000, 1000000);
    if (maxTransactions3 > maxTransactions) {
        std::cerr << "calculateMaxTransactions with higher bytes per transaction returned more transactions" << std::endl;
        return false;
    }
    
    std::cout << "Memory utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testThreadUtils();
    allTestsPassed &= testProcessUtils();
    allTestsPassed &= testMemoryUtils();
    
    if (allTestsPassed) {
        std::cout << "All System Utils tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some System Utils tests failed!" << std::endl;
        return 1;
    }
}
