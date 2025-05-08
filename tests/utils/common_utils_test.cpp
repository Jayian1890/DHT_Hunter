#include "common_utils.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

namespace dht_hunter::utils::tests {

// String utilities tests
TEST(StringUtilsTest, TrimLeft) {
    EXPECT_EQ(string::trimLeft("  hello"), "hello");
    EXPECT_EQ(string::trimLeft("hello"), "hello");
    EXPECT_EQ(string::trimLeft("  hello  "), "hello  ");
    EXPECT_EQ(string::trimLeft(""), "");
    EXPECT_EQ(string::trimLeft("  "), "");
}

TEST(StringUtilsTest, TrimRight) {
    EXPECT_EQ(string::trimRight("hello  "), "hello");
    EXPECT_EQ(string::trimRight("hello"), "hello");
    EXPECT_EQ(string::trimRight("  hello  "), "  hello");
    EXPECT_EQ(string::trimRight(""), "");
    EXPECT_EQ(string::trimRight("  "), "");
}

TEST(StringUtilsTest, Trim) {
    EXPECT_EQ(string::trim("  hello  "), "hello");
    EXPECT_EQ(string::trim("hello"), "hello");
    EXPECT_EQ(string::trim("  hello"), "hello");
    EXPECT_EQ(string::trim("hello  "), "hello");
    EXPECT_EQ(string::trim(""), "");
    EXPECT_EQ(string::trim("  "), "");
}

TEST(StringUtilsTest, Split) {
    std::vector<std::string> expected = {"hello", "world", "test"};
    EXPECT_EQ(string::split("hello,world,test", ','), expected);

    expected = {"hello"};
    EXPECT_EQ(string::split("hello", ','), expected);

    expected = {};
    EXPECT_EQ(string::split("", ','), expected);
}

TEST(StringUtilsTest, Join) {
    std::vector<std::string> tokens = {"hello", "world", "test"};
    EXPECT_EQ(string::join(tokens, ","), "hello,world,test");

    tokens = {"hello"};
    EXPECT_EQ(string::join(tokens, ","), "hello");

    tokens = {};
    EXPECT_EQ(string::join(tokens, ","), "");
}

TEST(StringUtilsTest, ToLower) {
    EXPECT_EQ(string::toLower("HELLO"), "hello");
    EXPECT_EQ(string::toLower("Hello"), "hello");
    EXPECT_EQ(string::toLower("hello"), "hello");
    EXPECT_EQ(string::toLower(""), "");
}

TEST(StringUtilsTest, ToUpper) {
    EXPECT_EQ(string::toUpper("hello"), "HELLO");
    EXPECT_EQ(string::toUpper("Hello"), "HELLO");
    EXPECT_EQ(string::toUpper("HELLO"), "HELLO");
    EXPECT_EQ(string::toUpper(""), "");
}

TEST(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(string::startsWith("hello world", "hello"));
    EXPECT_TRUE(string::startsWith("hello", "hello"));
    EXPECT_FALSE(string::startsWith("hello world", "world"));
    EXPECT_FALSE(string::startsWith("", "hello"));
    EXPECT_TRUE(string::startsWith("hello", ""));
}

TEST(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(string::endsWith("hello world", "world"));
    EXPECT_TRUE(string::endsWith("world", "world"));
    EXPECT_FALSE(string::endsWith("hello world", "hello"));
    EXPECT_FALSE(string::endsWith("", "world"));
    EXPECT_TRUE(string::endsWith("world", ""));
}

TEST(StringUtilsTest, ReplaceAll) {
    EXPECT_EQ(string::replaceAll("hello world", "world", "universe"), "hello universe");
    EXPECT_EQ(string::replaceAll("hello hello", "hello", "hi"), "hi hi");
    EXPECT_EQ(string::replaceAll("hello world", "universe", "galaxy"), "hello world");
    EXPECT_EQ(string::replaceAll("", "hello", "hi"), "");
}

// Hash utilities tests
TEST(HashUtilsTest, Sha1) {
    std::string input = "hello world";
    std::vector<uint8_t> expected = {
        0x2a, 0xae, 0x6c, 0x35, 0xc9, 0x4f, 0xcf, 0xb4, 0x15, 0xdb,
        0xe9, 0x5f, 0x40, 0x8b, 0x9c, 0xe9, 0x1e, 0xe8, 0x46, 0xed
    };

    std::vector<uint8_t> result = hash::sha1(input);
    EXPECT_EQ(result, expected);

    std::string hexResult = hash::sha1Hex(input);
    EXPECT_EQ(hexResult, "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed");
}

// File utilities tests
TEST(FileUtilsTest, ReadWriteFile) {
    // Create a temporary file
    std::string tempFilePath = "temp_test_file.txt";
    std::string content = "Hello, world!";

    // Write to the file
    auto writeResult = file::writeFile(tempFilePath, content);
    EXPECT_TRUE(writeResult.isSuccess());

    // Read from the file
    auto readResult = file::readFile(tempFilePath);
    EXPECT_TRUE(readResult.isSuccess());
    EXPECT_EQ(readResult.getValue(), content);

    // Check if the file exists
    EXPECT_TRUE(file::fileExists(tempFilePath));

    // Get the file size
    EXPECT_EQ(file::getFileSize(tempFilePath), content.size());

    // Clean up
    std::filesystem::remove(tempFilePath);
}

TEST(FileUtilsTest, CreateDirectory) {
    // Create a temporary directory
    std::string tempDirPath = "temp_test_dir";

    // Create the directory
    auto result = file::createDirectory(tempDirPath);
    EXPECT_TRUE(result.isSuccess());

    // Check if the directory exists
    EXPECT_TRUE(std::filesystem::exists(tempDirPath));

    // Clean up
    std::filesystem::remove_all(tempDirPath);
}

} // namespace dht_hunter::utils::tests
