#include "dht_hunter/network/rate_limiter.hpp"
#include "gtest/gtest.h"
#include <thread>
#include <chrono>
#include <vector>
#include <dht_hunter/logforge/logforge.hpp>

using namespace dht_hunter::network;
using namespace std::chrono_literals;

// Test basic rate limiting functionality
TEST(RateLimiterTest, BasicRateLimiting) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.RateLimiterTest");
    logger->info("Testing basic rate limiting");

    // Create a rate limiter with 1000 bytes per second
    RateLimiter limiter(1000);
    
    // Should be able to request 500 bytes immediately
    EXPECT_TRUE(limiter.request(500));
    
    // Should be able to request another 500 bytes
    EXPECT_TRUE(limiter.request(500));
    
    // Should not be able to request more bytes immediately
    EXPECT_FALSE(limiter.request(100));
    
    // Wait for tokens to replenish
    std::this_thread::sleep_for(200ms);
    
    // Should be able to request some bytes now (approximately 200)
    EXPECT_TRUE(limiter.request(150));
    EXPECT_FALSE(limiter.request(100));
}

// Test rate adjustment
TEST(RateLimiterTest, RateAdjustment) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.RateLimiterTest");
    logger->info("Testing rate adjustment");

    // Create a rate limiter with 1000 bytes per second
    RateLimiter limiter(1000);
    
    // Use up all tokens
    EXPECT_TRUE(limiter.request(1000));
    EXPECT_FALSE(limiter.request(1));
    
    // Increase the rate to 2000 bytes per second
    limiter.setRate(2000);
    
    // Wait for tokens to replenish at the new rate
    std::this_thread::sleep_for(500ms);
    
    // Should have approximately 1000 tokens now
    EXPECT_TRUE(limiter.request(900));
    EXPECT_FALSE(limiter.request(200));
}

// Test burst handling
TEST(RateLimiterTest, BurstHandling) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.RateLimiterTest");
    logger->info("Testing burst handling");

    // Create a rate limiter with 1000 bytes per second and max burst of 2000
    RateLimiter limiter(1000, 2000);
    
    // Wait to accumulate tokens
    std::this_thread::sleep_for(1500ms);
    
    // Should be able to request more than 1 second worth of tokens (burst)
    EXPECT_TRUE(limiter.request(1500));
    
    // Should have some tokens left
    EXPECT_TRUE(limiter.request(300));
    
    // But not more than the max burst
    EXPECT_FALSE(limiter.request(300));
}

// Test concurrent requests
TEST(RateLimiterTest, ConcurrentRequests) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.RateLimiterTest");
    logger->info("Testing concurrent requests");

    // Create a rate limiter with 10000 bytes per second
    RateLimiter limiter(10000);
    
    // Vector to store thread results
    std::vector<bool> results(10, false);
    
    // Create 10 threads that each request 1000 bytes
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 10; i++) {
        threads.emplace_back([&limiter, &results, i]() {
            results[i] = limiter.request(1000);
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Count how many requests succeeded
    int successCount = 0;
    for (bool result : results) {
        if (result) successCount++;
    }
    
    // All requests should succeed since they total exactly the rate limit
    EXPECT_EQ(successCount, 10);
    
    // An additional request should fail immediately after
    EXPECT_FALSE(limiter.request(1));
}

// Required for GoogleTest
int main(int argc, char **argv) {
    // Initialize with synchronous logging (async=false)
    dht_hunter::logforge::LogForge::init(
        dht_hunter::logforge::LogLevel::INFO,
        dht_hunter::logforge::LogLevel::DEBUG,
        "rate_limiter_test.log",
        true,  // useColors
        false  // async - set to false to avoid potential deadlocks
    );

    // Initialize GoogleTest
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
