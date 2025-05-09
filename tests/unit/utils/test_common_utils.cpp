#include "utils/common_utils.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cassert>

using namespace dht_hunter::utility;

/**
 * @brief Test the Result class
 * @return True if all tests pass, false otherwise
 */
bool testResult() {
    std::cout << "Testing Result class..." << std::endl;
    
    // Test successful result with value
    common::Result<int> successResult(42);
    if (!successResult.isSuccess()) {
        std::cerr << "Result with value should be successful" << std::endl;
        return false;
    }
    if (successResult.isError()) {
        std::cerr << "Result with value should not be an error" << std::endl;
        return false;
    }
    if (successResult.getValue() != 42) {
        std::cerr << "Result value is incorrect" << std::endl;
        return false;
    }
    
    // Test failed result with error message
    common::Result<int> errorResult = common::Result<int>::Error("Test error");
    if (errorResult.isSuccess()) {
        std::cerr << "Result with error should not be successful" << std::endl;
        return false;
    }
    if (!errorResult.isError()) {
        std::cerr << "Result with error should be an error" << std::endl;
        return false;
    }
    if (errorResult.getError() != "Test error") {
        std::cerr << "Result error message is incorrect" << std::endl;
        return false;
    }
    
    // Test void result
    common::Result<void> voidSuccessResult;
    if (!voidSuccessResult.isSuccess()) {
        std::cerr << "Void result should be successful by default" << std::endl;
        return false;
    }
    
    common::Result<void> voidErrorResult = common::Result<void>::Error("Void error");
    if (voidErrorResult.isSuccess()) {
        std::cerr << "Void result with error should not be successful" << std::endl;
        return false;
    }
    if (voidErrorResult.getError() != "Void error") {
        std::cerr << "Void result error message is incorrect" << std::endl;
        return false;
    }
    
    std::cout << "Result class tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the string utilities
 * @return True if all tests pass, false otherwise
 */
bool testStringUtils() {
    std::cout << "Testing string utilities..." << std::endl;
    
    // Test bytesToHex
    uint8_t bytes[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::string hexString = string::bytesToHex(bytes, sizeof(bytes));
    if (hexString != "0123456789abcdef") {
        std::cerr << "bytesToHex failed" << std::endl;
        std::cerr << "Expected: 0123456789abcdef, Got: " << hexString << std::endl;
        return false;
    }
    
    // Test formatHex with separator
    std::string formattedHex = string::formatHex(bytes, sizeof(bytes), ":");
    if (formattedHex != "01:23:45:67:89:ab:cd:ef") {
        std::cerr << "formatHex with separator failed" << std::endl;
        std::cerr << "Expected: 01:23:45:67:89:ab:cd:ef, Got: " << formattedHex << std::endl;
        return false;
    }
    
    // Test truncateString
    std::string longString = "This is a very long string that should be truncated";
    std::string truncated = string::truncateString(longString, 10);
    if (truncated != "This is a ...") {
        std::cerr << "truncateString failed" << std::endl;
        std::cerr << "Expected: 'This is a ...', Got: '" << truncated << "'" << std::endl;
        return false;
    }
    
    // Test truncateString with short string
    std::string shortString = "Short";
    std::string notTruncated = string::truncateString(shortString, 10);
    if (notTruncated != shortString) {
        std::cerr << "truncateString with short string failed" << std::endl;
        std::cerr << "Expected: '" << shortString << "', Got: '" << notTruncated << "'" << std::endl;
        return false;
    }
    
    std::cout << "String utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the hash utilities
 * @return True if all tests pass, false otherwise
 */
bool testHashUtils() {
    std::cout << "Testing hash utilities..." << std::endl;
    
    // Test SHA1 with string
    std::string testString = "test";
    std::string sha1String = hash::sha1Hex(testString);
    if (sha1String.length() != 40) {
        std::cerr << "SHA1 hash length is incorrect" << std::endl;
        std::cerr << "Expected: 40, Got: " << sha1String.length() << std::endl;
        return false;
    }
    
    // Test SHA1 with vector
    std::vector<uint8_t> testVector = {0x01, 0x02, 0x03, 0x04};
    std::string sha1Vector = hash::sha1Hex(testVector);
    if (sha1Vector.length() != 40) {
        std::cerr << "SHA1 hash length is incorrect" << std::endl;
        std::cerr << "Expected: 40, Got: " << sha1Vector.length() << std::endl;
        return false;
    }
    
    // Test SHA1 with raw data
    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    std::string sha1Data = hash::sha1Hex(testData, sizeof(testData));
    if (sha1Data.length() != 40) {
        std::cerr << "SHA1 hash length is incorrect" << std::endl;
        std::cerr << "Expected: 40, Got: " << sha1Data.length() << std::endl;
        return false;
    }
    
    std::cout << "Hash utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the file utilities
 * @return True if all tests pass, false otherwise
 */
bool testFileUtils() {
    std::cout << "Testing file utilities..." << std::endl;
    
    // Create a temporary directory for testing
    std::string testDir = "test_file_utils";
    if (!file::createDirectory(testDir)) {
        std::cerr << "Failed to create test directory" << std::endl;
        return false;
    }
    
    // Test fileExists
    if (!file::fileExists(testDir)) {
        std::cerr << "fileExists failed for directory" << std::endl;
        return false;
    }
    
    // Test createDirectory
    std::string nestedDir = testDir + "/nested";
    if (!file::createDirectory(nestedDir)) {
        std::cerr << "Failed to create nested directory" << std::endl;
        return false;
    }
    
    // Test writeFile
    std::string testFile = testDir + "/test.txt";
    std::string testContent = "This is a test file";
    auto writeResult = file::writeFile(testFile, testContent);
    if (!writeResult.isSuccess()) {
        std::cerr << "Failed to write test file: " << writeResult.getError() << std::endl;
        return false;
    }
    
    // Test fileExists for file
    if (!file::fileExists(testFile)) {
        std::cerr << "fileExists failed for file" << std::endl;
        return false;
    }
    
    // Test getFileSize
    size_t fileSize = file::getFileSize(testFile);
    if (fileSize != testContent.length()) {
        std::cerr << "getFileSize failed" << std::endl;
        std::cerr << "Expected: " << testContent.length() << ", Got: " << fileSize << std::endl;
        return false;
    }
    
    // Test readFile
    auto readResult = file::readFile(testFile);
    if (!readResult.isSuccess()) {
        std::cerr << "Failed to read test file: " << readResult.getError() << std::endl;
        return false;
    }
    if (readResult.getValue() != testContent) {
        std::cerr << "readFile returned incorrect content" << std::endl;
        std::cerr << "Expected: '" << testContent << "', Got: '" << readResult.getValue() << "'" << std::endl;
        return false;
    }
    
    // Clean up
    std::filesystem::remove_all(testDir);
    
    std::cout << "File utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the filesystem utilities
 * @return True if all tests pass, false otherwise
 */
bool testFilesystemUtils() {
    std::cout << "Testing filesystem utilities..." << std::endl;
    
    // Create a temporary directory structure for testing
    std::string testDir = "test_filesystem_utils";
    if (!file::createDirectory(testDir)) {
        std::cerr << "Failed to create test directory" << std::endl;
        return false;
    }
    
    std::string subDir1 = testDir + "/subdir1";
    std::string subDir2 = testDir + "/subdir2";
    if (!file::createDirectory(subDir1) || !file::createDirectory(subDir2)) {
        std::cerr << "Failed to create subdirectories" << std::endl;
        return false;
    }
    
    // Create some test files
    file::writeFile(testDir + "/file1.txt", "File 1");
    file::writeFile(subDir1 + "/file2.txt", "File 2");
    file::writeFile(subDir2 + "/file3.txt", "File 3");
    
    // Test getFiles (non-recursive)
    auto filesResult = filesystem::getFiles(testDir, false);
    if (!filesResult.isSuccess()) {
        std::cerr << "getFiles failed: " << filesResult.getError() << std::endl;
        return false;
    }
    if (filesResult.getValue().size() != 1) {
        std::cerr << "getFiles returned incorrect number of files" << std::endl;
        std::cerr << "Expected: 1, Got: " << filesResult.getValue().size() << std::endl;
        return false;
    }
    
    // Test getFiles (recursive)
    auto recursiveFilesResult = filesystem::getFiles(testDir, true);
    if (!recursiveFilesResult.isSuccess()) {
        std::cerr << "getFiles (recursive) failed: " << recursiveFilesResult.getError() << std::endl;
        return false;
    }
    if (recursiveFilesResult.getValue().size() != 3) {
        std::cerr << "getFiles (recursive) returned incorrect number of files" << std::endl;
        std::cerr << "Expected: 3, Got: " << recursiveFilesResult.getValue().size() << std::endl;
        return false;
    }
    
    // Test getDirectories (non-recursive)
    auto dirsResult = filesystem::getDirectories(testDir, false);
    if (!dirsResult.isSuccess()) {
        std::cerr << "getDirectories failed: " << dirsResult.getError() << std::endl;
        return false;
    }
    if (dirsResult.getValue().size() != 2) {
        std::cerr << "getDirectories returned incorrect number of directories" << std::endl;
        std::cerr << "Expected: 2, Got: " << dirsResult.getValue().size() << std::endl;
        return false;
    }
    
    // Test getFileExtension
    std::string ext = filesystem::getFileExtension(testDir + "/file1.txt");
    if (ext != "txt") {
        std::cerr << "getFileExtension failed" << std::endl;
        std::cerr << "Expected: 'txt', Got: '" << ext << "'" << std::endl;
        return false;
    }
    
    // Test getFileName
    std::string fileName = filesystem::getFileName(testDir + "/file1.txt");
    if (fileName != "file1") {
        std::cerr << "getFileName failed" << std::endl;
        std::cerr << "Expected: 'file1', Got: '" << fileName << "'" << std::endl;
        return false;
    }
    
    // Test getDirectoryPath
    std::string dirPath = filesystem::getDirectoryPath(testDir + "/file1.txt");
    if (dirPath != testDir) {
        std::cerr << "getDirectoryPath failed" << std::endl;
        std::cerr << "Expected: '" << testDir << "', Got: '" << dirPath << "'" << std::endl;
        return false;
    }
    
    // Clean up
    std::filesystem::remove_all(testDir);
    
    std::cout << "Filesystem utilities tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testResult();
    allTestsPassed &= testStringUtils();
    allTestsPassed &= testHashUtils();
    allTestsPassed &= testFileUtils();
    allTestsPassed &= testFilesystemUtils();
    
    if (allTestsPassed) {
        std::cout << "All Common Utils tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some Common Utils tests failed!" << std::endl;
        return 1;
    }
}
