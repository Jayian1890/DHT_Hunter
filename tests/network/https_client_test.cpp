#include "dht_hunter/network/http_client.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <iostream>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <functional>

using namespace dht_hunter::network;

// Simple test framework
class TestRunner {
public:
    void addTest(const std::string& name, std::function<bool()> test) {
        m_tests.push_back({name, test});
    }

    bool runTests() {
        std::cout << "Running " << m_tests.size() << " tests...\n" << std::endl;

        int passed = 0;
        int failed = 0;

        for (const auto& test : m_tests) {
            std::cout << "Test: " << test.name << std::endl;

            bool result = false;
            try {
                result = test.test();
            } catch (const std::exception& e) {
                std::cerr << "  Exception: " << e.what() << std::endl;
                result = false;
            } catch (...) {
                std::cerr << "  Unknown exception" << std::endl;
                result = false;
            }

            if (result) {
                std::cout << "  PASSED" << std::endl;
                passed++;
            } else {
                std::cerr << "  FAILED" << std::endl;
                failed++;
            }

            std::cout << std::endl;
        }

        std::cout << "Test results: " << passed << " passed, " << failed << " failed" << std::endl;

        return failed == 0;
    }

private:
    struct Test {
        std::string name;
        std::function<bool()> test;
    };

    std::vector<Test> m_tests;
};

// Test class for HTTPS client
class HttpsClientTest {
public:
    HttpsClientTest() {
        // Initialize the event system
        dht_hunter::unified_event::initializeEventSystem(true, true, true, false);

        // Set up the HTTP client
        m_httpClient.setUserAgent("BitScrape-Test/1.0");
        m_httpClient.setConnectionTimeout(15);
    }

    ~HttpsClientTest() {
        // Clean up resources
        // Sleep briefly to allow any pending operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // We won't call shutdownEventSystem() here to avoid mutex issues
        // The process will exit anyway, so resources will be cleaned up by the OS
    }

    // Test URL parsing with valid HTTP URL
    bool testParseHttpUrl() {
        std::cout << "Testing URL parsing with valid HTTP URL..." << std::endl;

        // Make a request to a valid HTTP URL
        // We don't care about the result, just that the URL is parsed correctly
        bool requestStarted = m_httpClient.get("http://example.com/path?query=value", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Test URL parsing with valid HTTPS URL
    bool testParseHttpsUrl() {
        std::cout << "Testing URL parsing with valid HTTPS URL..." << std::endl;

        // Make a request to a valid HTTPS URL
        // We don't care about the result, just that the URL is parsed correctly
        bool requestStarted = m_httpClient.get("https://example.com:443/path?query=value", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Test URL parsing with invalid URL
    bool testParseInvalidUrl() {
        std::cout << "Testing URL parsing with invalid URL..." << std::endl;

        // Make a request to an invalid URL
        bool requestStarted = m_httpClient.get("invalid://example.com", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should fail to start
        return !requestStarted;
    }

    // Test URL parsing with empty URL
    bool testParseEmptyUrl() {
        std::cout << "Testing URL parsing with empty URL..." << std::endl;

        // Make a request to an empty URL
        bool requestStarted = m_httpClient.get("", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should fail to start
        return !requestStarted;
    }

    // Test setting and getting error callback
    bool testErrorCallback() {
        std::cout << "Testing error callback..." << std::endl;

        // Set an error callback
        bool callbackCalled = false;
        m_httpClient.setErrorCallback([&callbackCalled](const std::string& error) {
            std::cout << "  Error callback called with: " << error << std::endl;
            callbackCalled = true;
        });

        // Get the error callback
        auto callback = m_httpClient.getErrorCallback();

        // The callback should not be null
        if (!callback) {
            std::cerr << "  Error callback is null" << std::endl;
            return false;
        }

        // Call the callback
        callback("Test error");

        // The callback should have been called
        return callbackCalled;
    }

    // Test setting user agent
    bool testSetUserAgent() {
        std::cout << "Testing setting user agent..." << std::endl;

        // Set a user agent
        m_httpClient.setUserAgent("TestUserAgent/1.0");

        // We can't directly test that the user agent was set correctly
        // since there's no getter method, but we can make a request and
        // check that it doesn't crash
        bool requestStarted = m_httpClient.get("http://example.com", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Test setting connection timeout
    bool testSetConnectionTimeout() {
        std::cout << "Testing setting connection timeout..." << std::endl;

        // Set a connection timeout
        m_httpClient.setConnectionTimeout(5);

        // We can't directly test that the timeout was set correctly
        // since there's no getter method, but we can make a request and
        // check that it doesn't crash
        bool requestStarted = m_httpClient.get("http://example.com", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Test setting request timeout
    bool testSetRequestTimeout() {
        std::cout << "Testing setting request timeout..." << std::endl;

        // Set a request timeout
        m_httpClient.setRequestTimeout(15);

        // We can't directly test that the timeout was set correctly
        // since there's no getter method, but we can make a request and
        // check that it doesn't crash
        bool requestStarted = m_httpClient.get("http://example.com", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Test setting max redirects
    bool testSetMaxRedirects() {
        std::cout << "Testing setting max redirects..." << std::endl;

        // Set max redirects
        m_httpClient.setMaxRedirects(3);

        // We can't directly test that the max redirects was set correctly
        // since there's no getter method, but we can make a request and
        // check that it doesn't crash
        bool requestStarted = m_httpClient.get("http://example.com", [](bool, const HttpClientResponse&) {
            // Ignore the callback
        });

        // The request should start successfully
        return requestStarted;
    }

    // Run all tests
    bool runAllTests() {
        TestRunner runner;

        // Add tests
        runner.addTest("Parse HTTP URL", [this]() { return testParseHttpUrl(); });
        runner.addTest("Parse HTTPS URL", [this]() { return testParseHttpsUrl(); });
        runner.addTest("Parse Invalid URL", [this]() { return testParseInvalidUrl(); });
        runner.addTest("Parse Empty URL", [this]() { return testParseEmptyUrl(); });
        runner.addTest("Error Callback", [this]() { return testErrorCallback(); });
        runner.addTest("Set User Agent", [this]() { return testSetUserAgent(); });
        runner.addTest("Set Connection Timeout", [this]() { return testSetConnectionTimeout(); });
        runner.addTest("Set Request Timeout", [this]() { return testSetRequestTimeout(); });
        runner.addTest("Set Max Redirects", [this]() { return testSetMaxRedirects(); });

        // Run tests
        return runner.runTests();
    }

private:
    HttpClient m_httpClient;
};

int main() {
    HttpsClientTest test;
    bool success = test.runAllTests();

    // Return 0 for success, 1 for failure
    return success ? 0 : 1;
}
