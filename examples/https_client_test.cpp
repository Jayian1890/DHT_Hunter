#include "dht_hunter/network/http_client.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <cassert>

using namespace dht_hunter;
using namespace dht_hunter::network;

// Test class to run HTTPS client tests
class HttpsClientTest {
public:
    HttpsClientTest() : m_httpClient() {
        // Initialize the event system
        unified_event::initializeEventSystem(true, true, true, false);

        // Set up the HTTP client
        m_httpClient.setUserAgent("BitScrape-Test/1.0");
        m_httpClient.setConnectionTimeout(15);
        // Note: setVerifyCertificates is private, so we can't call it directly
        // The default is true, which is what we want for most tests
    }

    ~HttpsClientTest() {
        // Shutdown the event system
        unified_event::shutdownEventSystem();
    }

    // Test a basic HTTPS GET request to GitHub
    bool testHttpsGet() {
        std::cout << "Running HTTPS GET test to GitHub..." << std::endl;

        bool success = false;
        std::mutex mutex;
        std::condition_variable cv;
        bool requestComplete = false;
        HttpClientResponse response;

        // Make a GET request to GitHub's API
        m_httpClient.get("https://api.github.com/", [&](bool requestSuccess, const HttpClientResponse& resp) {
            std::lock_guard<std::mutex> lock(mutex);
            success = requestSuccess;
            response = resp;
            requestComplete = true;
            cv.notify_one();
        });

        // Wait for the request to complete
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(30), [&]() { return requestComplete; })) {
            std::cerr << "Request timed out" << std::endl;
            return false;
        }

        // Check if the request was successful
        if (!success) {
            std::cerr << "Request failed" << std::endl;
            return false;
        }

        // Check if we got a valid response
        if (response.statusCode != 200) {
            std::cerr << "Unexpected status code: " << response.statusCode << std::endl;
            return false;
        }

        // Check if we got a response body
        if (response.body.empty()) {
            std::cerr << "Empty response body" << std::endl;
            return false;
        }

        // Print some information about the response
        std::cout << "Status code: " << response.statusCode << std::endl;
        std::cout << "Headers: " << response.headers.size() << std::endl;
        std::cout << "Body length: " << response.body.length() << " bytes" << std::endl;

        // Print the first 100 characters of the response body
        std::cout << "Body preview: " << response.body.substr(0, 100) << "..." << std::endl;

        return true;
    }

    // Test HTTPS with a valid certificate
    bool testValidCertificate() {
        std::cout << "Running valid certificate test..." << std::endl;

        bool success = false;
        std::mutex mutex;
        std::condition_variable cv;
        bool requestComplete = false;

        m_httpClient.get("https://github.com/", [&](bool requestSuccess, const HttpClientResponse&) {
            std::lock_guard<std::mutex> lock(mutex);
            success = requestSuccess;
            requestComplete = true;
            cv.notify_one();
        });

        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(30), [&]() { return requestComplete; })) {
            std::cerr << "Request with valid certificate timed out" << std::endl;
            return false;
        }

        if (!success) {
            std::cerr << "Request with valid certificate failed" << std::endl;
            return false;
        }

        std::cout << "Request with valid certificate succeeded" << std::endl;
        return true;
    }

    // Test HTTP connection to localhost
    bool testLocalhost() {
        std::cout << "Running localhost connection test..." << std::endl;

        // This test requires a local HTTP server to be running
        // We'll try to connect to port 8080, which is the default port for the DHT Hunter web server
        // If no server is running, this test will fail, but that's expected

        bool success = false;
        std::mutex mutex;
        std::condition_variable cv;
        bool requestComplete = false;

        m_httpClient.get("http://localhost:8080/", [&](bool requestSuccess, const HttpClientResponse& response) {
            std::lock_guard<std::mutex> lock(mutex);
            success = requestSuccess;
            if (success) {
                std::cout << "Received response from localhost:8080 with status code: " << response.statusCode << std::endl;
            }
            requestComplete = true;
            cv.notify_one();
        });

        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&]() { return requestComplete; })) {
            std::cerr << "Localhost connection test timed out" << std::endl;
            return false;
        }

        // We don't check success here because it depends on whether a server is running
        std::cout << "Localhost connection test completed with result: " << (success ? "SUCCESS" : "FAILURE") << std::endl;
        return true;
    }

    // Test HTTPS POST request
    bool testHttpsPost() {
        std::cout << "Running HTTPS POST test..." << std::endl;

        bool success = false;
        std::mutex mutex;
        std::condition_variable cv;
        bool requestComplete = false;
        HttpClientResponse response;

        // Create a simple JSON payload
        std::string body = "{\"name\":\"test\",\"value\":123}";

        // Make a POST request to httpbin.org
        m_httpClient.post("https://httpbin.org/post", "application/json", body, [&](bool requestSuccess, const HttpClientResponse& resp) {
            std::lock_guard<std::mutex> lock(mutex);
            success = requestSuccess;
            response = resp;
            requestComplete = true;
            cv.notify_one();
        });

        // Wait for the request to complete
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(30), [&]() { return requestComplete; })) {
            std::cerr << "POST request timed out" << std::endl;
            return false;
        }

        // Check if the request was successful
        if (!success) {
            std::cerr << "POST request failed" << std::endl;
            return false;
        }

        // Check if we got a valid response
        if (response.statusCode != 200) {
            std::cerr << "Unexpected status code: " << response.statusCode << std::endl;
            return false;
        }

        // Check if we got a response body
        if (response.body.empty()) {
            std::cerr << "Empty response body" << std::endl;
            return false;
        }

        // Print some information about the response
        std::cout << "Status code: " << response.statusCode << std::endl;
        std::cout << "Headers: " << response.headers.size() << std::endl;
        std::cout << "Body length: " << response.body.length() << " bytes" << std::endl;

        // Print the first 100 characters of the response body
        std::cout << "Body preview: " << response.body.substr(0, 100) << "..." << std::endl;

        // Check if our data was echoed back
        if (response.body.find(body) == std::string::npos) {
            std::cerr << "Response body does not contain the request body" << std::endl;
            return false;
        }

        return true;
    }

    // Test error handling for invalid URLs
    bool testInvalidUrl() {
        std::cout << "Running invalid URL test..." << std::endl;

        // Test with an invalid URL scheme
        {
            bool success = true; // Should be set to false by the callback
            std::mutex mutex;
            std::condition_variable cv;
            bool requestComplete = false;

            m_httpClient.get("ftp://example.com/", [&](bool requestSuccess, const HttpClientResponse&) {
                std::lock_guard<std::mutex> lock(mutex);
                success = requestSuccess;
                requestComplete = true;
                cv.notify_one();
            });

            std::unique_lock<std::mutex> lock(mutex);
            if (!cv.wait_for(lock, std::chrono::seconds(5), [&]() { return requestComplete; })) {
                std::cerr << "Invalid URL scheme test timed out" << std::endl;
                return false;
            }

            if (success) {
                std::cerr << "Invalid URL scheme test should have failed but succeeded" << std::endl;
                return false;
            }

            std::cout << "Invalid URL scheme test passed" << std::endl;
        }

        // Test with a non-existent domain
        {
            bool success = true; // Should be set to false by the callback
            std::mutex mutex;
            std::condition_variable cv;
            bool requestComplete = false;

            m_httpClient.get("https://nonexistent-domain-12345.com/", [&](bool requestSuccess, const HttpClientResponse&) {
                std::lock_guard<std::mutex> lock(mutex);
                success = requestSuccess;
                requestComplete = true;
                cv.notify_one();
            });

            std::unique_lock<std::mutex> lock(mutex);
            if (!cv.wait_for(lock, std::chrono::seconds(15), [&]() { return requestComplete; })) {
                std::cerr << "Non-existent domain test timed out" << std::endl;
                return false;
            }

            if (success) {
                std::cerr << "Non-existent domain test should have failed but succeeded" << std::endl;
                return false;
            }

            std::cout << "Non-existent domain test passed" << std::endl;
        }

        return true;
    }

    // Run all tests
    bool runAllTests() {
        bool allPassed = true;

        // Test invalid URL (this should always work regardless of network connectivity)
        if (!testInvalidUrl()) {
            std::cerr << "Invalid URL test failed" << std::endl;
            allPassed = false;
        } else {
            std::cout << "Invalid URL test passed" << std::endl;
        }

        // The following tests require network connectivity
        std::cout << "\nNote: The following tests require network connectivity." << std::endl;
        std::cout << "If they fail, it might be due to network issues rather than code issues." << std::endl;

        // Test HTTPS GET
        bool getTestResult = testHttpsGet();
        if (!getTestResult) {
            std::cerr << "HTTPS GET test failed" << std::endl;
            // Don't fail the overall test for network-dependent tests
            // allPassed = false;
        } else {
            std::cout << "HTTPS GET test passed" << std::endl;
        }

        // Test valid certificate
        bool certTestResult = testValidCertificate();
        if (!certTestResult) {
            std::cerr << "Valid certificate test failed" << std::endl;
            // Don't fail the overall test for network-dependent tests
            // allPassed = false;
        } else {
            std::cout << "Valid certificate test passed" << std::endl;
        }

        // Test HTTPS POST
        bool postTestResult = testHttpsPost();
        if (!postTestResult) {
            std::cerr << "HTTPS POST test failed" << std::endl;
            // Don't fail the overall test for network-dependent tests
            // allPassed = false;
        } else {
            std::cout << "HTTPS POST test passed" << std::endl;
        }

        // Test localhost connection
        bool localhostTestResult = testLocalhost();
        if (!localhostTestResult) {
            std::cerr << "Localhost connection test failed" << std::endl;
            allPassed = false;
        } else {
            std::cout << "Localhost connection test passed" << std::endl;
        }

        // Print a summary of network-dependent tests
        std::cout << "\nNetwork-dependent test summary:" << std::endl;
        std::cout << "  HTTPS GET: " << (getTestResult ? "PASSED" : "FAILED") << std::endl;
        std::cout << "  Valid Certificate: " << (certTestResult ? "PASSED" : "FAILED") << std::endl;
        std::cout << "  HTTPS POST: " << (postTestResult ? "PASSED" : "FAILED") << std::endl;
        std::cout << "  Localhost Connection: " << (localhostTestResult ? "PASSED" : "FAILED") << std::endl;

        return allPassed;
    }

private:
    HttpClient m_httpClient;
};

int main() {
    std::cout << "Starting HTTPS client tests..." << std::endl;

    HttpsClientTest test;
    bool success = test.runAllTests();

    if (success) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}
