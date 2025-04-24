#include "dht_hunter/network/buffer_pool.hpp"
#include "gtest/gtest.h"
#include <string>
#include <vector>
#include <algorithm>
#include <dht_hunter/logforge/logforge.hpp>

using namespace dht_hunter::network;

// Test Buffer class basic operations
TEST(BufferTest, BufferBasicOperations) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing Buffer basic operations");

    // Create a buffer with initial size
    Buffer buffer(1024);
    EXPECT_EQ(buffer.size(), 1024);
    
    // Test data access
    uint8_t* data = buffer.data();
    EXPECT_NE(data, nullptr);
    
    // Test setting used size
    buffer.setUsedSize(512);
    EXPECT_EQ(buffer.getUsedSize(), 512);
    
    // Test clear operation
    buffer.clear();
    EXPECT_EQ(buffer.getUsedSize(), 0);
    
    // Test resize operation
    buffer.resize(2048);
    EXPECT_EQ(buffer.size(), 2048);
    EXPECT_EQ(buffer.getUsedSize(), 0);  // Used size should be reset
    
    // Test setting used size beyond buffer size
    buffer.setUsedSize(3000);  // Should be clamped to 2048
    EXPECT_EQ(buffer.getUsedSize(), 2048);
}

// Test MessageBuffer basic operations
TEST(BufferTest, MessageBufferBasicOperations) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer basic operations");

    // Create a buffer with initial capacity
    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(1024);
    MessageBuffer msgBuffer(buffer);
    EXPECT_EQ(msgBuffer.size(), 1024);
    EXPECT_EQ(msgBuffer.getUsedSize(), 0);
    
    // Write data to the buffer
    const std::string testData = "Hello, DHT-Hunter!";
    msgBuffer.write(reinterpret_cast<const uint8_t*>(testData.data()), testData.size());
    
    EXPECT_EQ(msgBuffer.getUsedSize(), testData.size());
    
    // Read data from the buffer
    std::vector<uint8_t> readData(testData.size());
    size_t bytesRead = msgBuffer.read(readData.data(), readData.size());
    
    EXPECT_EQ(bytesRead, testData.size());
    EXPECT_TRUE(std::equal(readData.begin(), readData.end(), testData.begin()));
    // NOTE: MessageBuffer's read() doesn't update getUsedSize(), so we don't check it here
}

// Test MessageBuffer resizing
TEST(BufferTest, MessageBufferResizing) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer resizing");

    // Create a small buffer
    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(16);
    MessageBuffer msgBuffer(buffer);
    EXPECT_EQ(msgBuffer.size(), 16);

    // Write data that exceeds the initial capacity
    std::vector<uint8_t> largeData(32, 0xAA);  // 32 bytes of 0xAA
    msgBuffer.write(largeData.data(), largeData.size());
    
    // Buffer should have automatically resized
    EXPECT_GE(msgBuffer.size(), 32);
    EXPECT_EQ(msgBuffer.getUsedSize(), 32);

    // Read the data back and verify
    std::vector<uint8_t> readData(32);
    size_t bytesRead = msgBuffer.read(readData.data(), readData.size());
    
    EXPECT_EQ(bytesRead, 32);
    EXPECT_TRUE(std::equal(readData.begin(), readData.end(), largeData.begin()));
}

// Test MessageBuffer non-destructive read operations
TEST(BufferTest, MessageBufferNonDestructiveRead) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer non-destructive read operations");

    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(1024);
    MessageBuffer msgBuffer(buffer);
    
    // Write test data
    const std::string testData = "DHT-Hunter Buffer Test";
    msgBuffer.write(reinterpret_cast<const uint8_t*>(testData.data()), testData.size());
    
    // Read a portion of the data
    std::vector<uint8_t> readData(10);  // Read only first 10 bytes
    size_t bytesRead = msgBuffer.read(readData.data(), readData.size());
    
    EXPECT_EQ(bytesRead, 10);
    EXPECT_TRUE(std::equal(readData.begin(), readData.end(), testData.begin()));
    
    // Read the remaining data
    std::vector<uint8_t> remainingData(testData.size() - 10);
    size_t remainingBytesRead = msgBuffer.read(remainingData.data(), remainingData.size());
    
    EXPECT_EQ(remainingBytesRead, testData.size() - 10);
    EXPECT_TRUE(std::equal(remainingData.begin(), remainingData.end(), testData.begin() + 10));
    // NOTE: MessageBuffer's read() doesn't update getUsedSize(), so we don't check it here
}

// Test MessageBuffer partial read/write operations
TEST(BufferTest, MessageBufferPartialOperations) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer partial read/write operations");

    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(1024);
    MessageBuffer msgBuffer(buffer);
    
    // Write multiple chunks of data
    const std::string chunk1 = "First chunk of data";
    const std::string chunk2 = "Second chunk of data";
    
    msgBuffer.write(reinterpret_cast<const uint8_t*>(chunk1.data()), chunk1.size());
    EXPECT_EQ(msgBuffer.getUsedSize(), chunk1.size());
    
    msgBuffer.write(reinterpret_cast<const uint8_t*>(chunk2.data()), chunk2.size());
    EXPECT_EQ(msgBuffer.getUsedSize(), chunk1.size() + chunk2.size());
    
    // Read first chunk
    std::vector<uint8_t> readData1(chunk1.size());
    size_t bytesRead1 = msgBuffer.read(readData1.data(), readData1.size());
    
    EXPECT_EQ(bytesRead1, chunk1.size());
    EXPECT_TRUE(std::equal(readData1.begin(), readData1.end(), chunk1.begin()));
    // NOTE: MessageBuffer's read() doesn't update getUsedSize()
    
    // Read second chunk
    std::vector<uint8_t> readData2(chunk2.size());
    size_t bytesRead2 = msgBuffer.read(readData2.data(), readData2.size());
    
    EXPECT_EQ(bytesRead2, chunk2.size());
    EXPECT_TRUE(std::equal(readData2.begin(), readData2.end(), chunk2.begin()));
    // NOTE: MessageBuffer's read() doesn't update getUsedSize()
}

// Test MessageBuffer clear operation
TEST(BufferTest, MessageBufferClearOperation) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer clear operation");

    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(1024);
    MessageBuffer msgBuffer(buffer);
    
    // Write test data
    std::vector<uint8_t> testData(512, 0xBB);
    msgBuffer.write(testData.data(), testData.size());
    EXPECT_EQ(msgBuffer.getUsedSize(), 512);
    
    // Clear the buffer
    msgBuffer.clear();
    EXPECT_EQ(msgBuffer.getUsedSize(), 0);
    EXPECT_EQ(msgBuffer.size(), 1024);  // Size should remain unchanged
    
    // Write new data after clearing
    msgBuffer.write(testData.data(), 256);
    EXPECT_EQ(msgBuffer.getUsedSize(), 256);
}

// Test reading more data than available
TEST(BufferTest, MessageBufferReadBeyondAvailable) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.BufferTest");
    logger->info("Testing MessageBuffer read beyond available data");

    std::shared_ptr<Buffer> buffer = std::make_shared<Buffer>(1024);
    MessageBuffer msgBuffer(buffer);
    
    // Write a small amount of data
    const std::string testData = "Small data";
    msgBuffer.write(reinterpret_cast<const uint8_t*>(testData.data()), testData.size());
    EXPECT_EQ(msgBuffer.getUsedSize(), testData.size());
    
    // Try to read more data than available
    std::vector<uint8_t> readData(100);  // Much larger than the data we wrote
    size_t bytesRead = msgBuffer.read(readData.data(), readData.size());
    
    // Should only read the available data
    EXPECT_EQ(bytesRead, testData.size());
    EXPECT_TRUE(std::equal(readData.begin(), readData.begin() + static_cast<long>(bytesRead), testData.begin()));
    // NOTE: MessageBuffer's read() doesn't update getUsedSize()
}

// Required for GoogleTest
int main(int argc, char **argv) {
    // Initialize with synchronous logging (async=false)
    dht_hunter::logforge::LogForge::init(
        dht_hunter::logforge::LogLevel::INFO,
        dht_hunter::logforge::LogLevel::DEBUG,
        "buffer_test.log",
        true,  // useColors
        false  // async - set to false to avoid potential deadlocks
    );

    // Initialize GoogleTest
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}