#include "utils/system_utils.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <future>

namespace dht_hunter::utils::tests {

// Thread utilities tests
TEST(ThreadUtilsTest, ThreadPool) {
    // Create a thread pool with 4 threads
    thread::ThreadPool pool(4);
    
    // Check the thread count
    EXPECT_EQ(pool.getThreadCount(), 4);
    
    // Test with a simple task
    auto future = pool.enqueue([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
    
    // Test with multiple tasks
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([i]() { 
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i; 
        }));
    }
    
    // Check the results
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(futures[i].get(), i);
    }
}

TEST(ThreadUtilsTest, Sleep) {
    // Test sleep function
    auto start = std::chrono::steady_clock::now();
    thread::sleep(100);
    auto end = std::chrono::steady_clock::now();
    
    // Check that at least 100ms have passed
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 100);
}

TEST(ThreadUtilsTest, ThreadId) {
    // Test getCurrentThreadId
    auto id1 = thread::getCurrentThreadId();
    
    // Get thread ID from another thread
    auto future = std::async(std::launch::async, []() {
        return thread::getCurrentThreadId();
    });
    
    auto id2 = future.get();
    
    // The IDs should be different
    EXPECT_NE(id1, id2);
}

// Process utilities tests
TEST(ProcessUtilsTest, ProcessId) {
    // Test getCurrentProcessId
    int pid = process::getCurrentProcessId();
    EXPECT_GT(pid, 0);
    
    // Test isProcessRunning
    EXPECT_TRUE(process::isProcessRunning(pid));
    
    // Test with an invalid PID
    EXPECT_FALSE(process::isProcessRunning(-1));
}

TEST(ProcessUtilsTest, ProcessName) {
    // Test getCurrentProcessName
    std::string name = process::getCurrentProcessName();
    EXPECT_FALSE(name.empty());
}

TEST(ProcessUtilsTest, WorkingDirectory) {
    // Test getCurrentWorkingDirectory
    std::string cwd = process::getCurrentWorkingDirectory();
    EXPECT_FALSE(cwd.empty());
    
    // Save the current directory
    std::string originalCwd = cwd;
    
    // Create a temporary directory
    std::string tempDir = "temp_test_dir";
    std::filesystem::create_directory(tempDir);
    
    // Change to the temporary directory
    EXPECT_TRUE(process::setCurrentWorkingDirectory(tempDir));
    
    // Check that the working directory has changed
    EXPECT_EQ(process::getCurrentWorkingDirectory(), std::filesystem::absolute(tempDir).string());
    
    // Change back to the original directory
    EXPECT_TRUE(process::setCurrentWorkingDirectory(originalCwd));
    
    // Clean up
    std::filesystem::remove_all(tempDir);
}

// Memory utilities tests
TEST(MemoryUtilsTest, MemoryUsage) {
    // Test getCurrentMemoryUsage
    size_t usage = memory::getCurrentMemoryUsage();
    EXPECT_GT(usage, 0);
    
    // Test getPeakMemoryUsage
    size_t peakUsage = memory::getPeakMemoryUsage();
    EXPECT_GT(peakUsage, 0);
    
    // Test getSystemMemoryInfo
    auto [totalMemory, availableMemory] = memory::getSystemMemoryInfo();
    EXPECT_GT(totalMemory, 0);
    EXPECT_GT(availableMemory, 0);
    EXPECT_LE(availableMemory, totalMemory);
}

} // namespace dht_hunter::utils::tests
