#pragma once

#include "dht_hunter/network/tcp_client.hpp"
#include "dht_hunter/network/ssl_client.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

namespace dht_hunter::network {

/**
 * @brief HTTP response structure
 */
struct HttpClientResponse {
    int statusCode{0};
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

/**
 * @brief HTTP client for making HTTP requests
 */
class HttpClient {
public:
    /**
     * @brief Constructor
     */
    HttpClient();

    /**
     * @brief Destructor
     */
    ~HttpClient();

    /**
     * @brief Performs an HTTP GET request
     * @param url The URL to request
     * @param callback The callback to call when the request is complete
     * @return True if the request was started, false otherwise
     */
    bool get(const std::string& url, std::function<void(bool success, const HttpClientResponse& response)> callback);

    /**
     * @brief Performs an HTTP POST request
     * @param url The URL to request
     * @param contentType The content type of the request body
     * @param body The request body
     * @param callback The callback to call when the request is complete
     * @return True if the request was started, false otherwise
     */
    bool post(const std::string& url, const std::string& contentType, const std::string& body,
              std::function<void(bool success, const HttpClientResponse& response)> callback);

    /**
     * @brief Sets the user agent for requests
     * @param userAgent The user agent string
     */
    void setUserAgent(const std::string& userAgent);

    /**
     * @brief Sets the connection timeout
     * @param timeoutSec The timeout in seconds
     */
    void setConnectionTimeout(int timeoutSec);

    /**
     * @brief Sets the request timeout
     * @param timeoutSec The timeout in seconds
     */
    void setRequestTimeout(int timeoutSec);

    /**
     * @brief Sets the maximum number of redirects to follow
     * @param maxRedirects The maximum number of redirects
     */
    void setMaxRedirects(int maxRedirects);

    /**
     * @brief Sets the error callback
     * @param callback The callback to call when an error occurs
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Gets the current error callback
     * @return The current error callback
     */
    std::function<void(const std::string&)> getErrorCallback() const;

private:
    /**
     * @brief Parses a URL into its components
     * @param url The URL to parse
     * @param scheme The scheme (output)
     * @param host The host (output)
     * @param port The port (output)
     * @param path The path (output)
     * @return True if the URL was parsed successfully, false otherwise
     */
    bool parseUrl(const std::string& url, std::string& scheme, std::string& host, uint16_t& port, std::string& path);

    /**
     * @brief Performs an HTTP request
     * @param method The HTTP method (GET, POST, etc.)
     * @param url The URL to request
     * @param headers The request headers
     * @param body The request body
     * @param callback The callback to call when the request is complete
     * @return True if the request was started, false otherwise
     */
    bool request(const std::string& method, const std::string& url,
                 const std::unordered_map<std::string, std::string>& headers,
                 const std::string& body,
                 std::function<void(bool success, const HttpClientResponse& response)> callback);

    /**
     * @brief Parses an HTTP response
     * @param data The response data
     * @param response The response structure to fill
     * @return True if the response was parsed successfully, false otherwise
     */
    bool parseResponse(const std::string& data, HttpClientResponse& response);

    /**
     * @brief Sets whether to verify SSL certificates
     * @param verify True to verify certificates, false to skip verification
     */
    void setVerifyCertificates(bool verify);

    std::string m_userAgent{"BitScrape/1.0"};
    int m_connectionTimeout{10};
    int m_requestTimeout{30};
    int m_maxRedirects{5};
    bool m_verifyCertificates{true};
    std::function<void(const std::string&)> m_errorCallback;
    std::mutex m_mutex;
};

} // namespace dht_hunter::network
