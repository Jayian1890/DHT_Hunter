#include "dht_hunter/network/http_client.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/network/network_utils.hpp"

#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <regex>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

namespace dht_hunter::network {

HttpClient::HttpClient() {
    // Initialize with default values
    m_userAgent = utility::network::getUserAgent();
}

HttpClient::~HttpClient() {
    // Nothing to clean up
}

bool HttpClient::get(const std::string& url, std::function<void(bool success, const HttpClientResponse& response)> callback) {
    return request("GET", url, {}, "", callback);
}

bool HttpClient::post(const std::string& url, const std::string& contentType, const std::string& body,
                      std::function<void(bool success, const HttpClientResponse& response)> callback) {
    std::unordered_map<std::string, std::string> headers;
    headers["Content-Type"] = contentType;
    headers["Content-Length"] = std::to_string(body.length());

    return request("POST", url, headers, body, callback);
}

void HttpClient::setUserAgent(const std::string& userAgent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_userAgent = userAgent;
}

void HttpClient::setConnectionTimeout(int timeoutSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionTimeout = timeoutSec;
}

void HttpClient::setRequestTimeout(int timeoutSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestTimeout = timeoutSec;
}

void HttpClient::setMaxRedirects(int maxRedirects) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxRedirects = maxRedirects;
}

void HttpClient::setVerifyCertificates(bool verify) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_verifyCertificates = verify;
}

bool HttpClient::parseUrl(const std::string& url, std::string& scheme, std::string& host, uint16_t& port, std::string& path) {
    // Parse the URL using regex
    std::regex urlRegex("(http|https)://([^/:]+)(:\\d+)?(/.*)?");
    std::smatch matches;

    if (!std::regex_match(url, matches, urlRegex)) {
        unified_event::logWarning("Network.HttpClient", "Invalid URL format: " + url);
        return false;
    }

    scheme = matches[1].str();
    host = matches[2].str();

    // Parse port
    if (matches[3].length() > 0) {
        std::string portStr = matches[3].str().substr(1); // Skip the colon
        port = static_cast<uint16_t>(std::stoi(portStr));
    } else {
        // Default ports
        port = (scheme == "https") ? 443 : 80;
    }

    // Parse path
    path = matches[4].length() > 0 ? matches[4].str() : "/";

    return true;
}

bool HttpClient::request(const std::string& method, const std::string& url,
                         const std::unordered_map<std::string, std::string>& headers,
                         const std::string& body,
                         std::function<void(bool success, const HttpClientResponse& response)> callback) {

    if (!callback) {
        unified_event::logWarning("Network.HttpClient", "Cannot make request - callback is null");
        return false;
    }

    unified_event::logDebug("Network.HttpClient", method + " " + url);

    // Parse the URL
    std::string scheme, host, path;
    uint16_t port;
    if (!parseUrl(url, scheme, host, port, path)) {
        callback(false, {});
        return false;
    }

    // Check if the scheme is supported
    if (scheme != "http" && scheme != "https") {
        unified_event::logWarning("Network.HttpClient", "Unsupported URL scheme: " + scheme);
        callback(false, {});
        return false;
    }

    // Create a thread to handle the request asynchronously
    std::thread([this, method, url, host, port, path, headers, body, callback, scheme]() {
        // Variables to hold the client and response data
        std::shared_ptr<TCPClient> tcpClient;
        std::shared_ptr<SSLClient> sslClient;
        std::string responseData;
        bool responseComplete = false;

        // Connect to the server using the appropriate client
        if (scheme == "https") {
            // Use SSL client for HTTPS
            unified_event::logDebug("Network.HttpClient", "Connecting to " + host + ":" + std::to_string(port) + " using SSL");
            sslClient = std::make_shared<SSLClient>();
            sslClient->setVerifyCertificate(m_verifyCertificates);

            if (!sslClient->connect(host, port, m_connectionTimeout)) {
                unified_event::logWarning("Network.HttpClient", "Failed to establish secure connection to " + host + ":" + std::to_string(port));
                callback(false, {});
                return;
            }

            // Set up the data received callback for SSL client
            sslClient->setDataReceivedCallback([&](const uint8_t* data, size_t length) {
                responseData.append(reinterpret_cast<const char*>(data), length);

                // Check if we have received the complete response
                if (responseData.find("\r\n\r\n") != std::string::npos) {
                    size_t headerEnd = responseData.find("\r\n\r\n") + 4;

                    // Check for Content-Length header
                    std::string headers = responseData.substr(0, headerEnd);
                    std::regex contentLengthRegex("Content-Length: (\\d+)", std::regex::icase);
                    std::smatch matches;

                    if (std::regex_search(headers, matches, contentLengthRegex)) {
                        size_t contentLength = std::stoul(matches[1].str());
                        if (responseData.length() >= headerEnd + contentLength) {
                            responseComplete = true;
                        }
                    }
                }
            });
        } else {
            // Use TCP client for HTTP
            unified_event::logDebug("Network.HttpClient", "Connecting to " + host + ":" + std::to_string(port));
            tcpClient = std::make_shared<TCPClient>();

            if (!tcpClient->connect(host, port, m_connectionTimeout)) {
                unified_event::logWarning("Network.HttpClient", "Failed to connect to " + host + ":" + std::to_string(port));
                callback(false, {});
                return;
            }

            // Set up the data received callback for TCP client
            tcpClient->setDataReceivedCallback([&](const uint8_t* data, size_t length) {
                responseData.append(reinterpret_cast<const char*>(data), length);

                // Check if we have received the complete response
                if (responseData.find("\r\n\r\n") != std::string::npos) {
                    size_t headerEnd = responseData.find("\r\n\r\n") + 4;

                    // Check for Content-Length header
                    std::string headers = responseData.substr(0, headerEnd);
                    std::regex contentLengthRegex("Content-Length: (\\d+)", std::regex::icase);
                    std::smatch matches;

                    if (std::regex_search(headers, matches, contentLengthRegex)) {
                        size_t contentLength = std::stoul(matches[1].str());
                        if (responseData.length() >= headerEnd + contentLength) {
                            responseComplete = true;
                        }
                    }
                }
            });
        }

        // Build the request
        std::stringstream requestStream;
        requestStream << method << " " << path << " HTTP/1.1\r\n";
        requestStream << "Host: " << host;
        if ((port != 80 && scheme == "http") || (port != 443 && scheme == "https")) {
            requestStream << ":" << port;
        }
        requestStream << "\r\n";
        requestStream << "User-Agent: " << m_userAgent << "\r\n";
        requestStream << "Connection: close\r\n";

        // Add custom headers
        for (const auto& header : headers) {
            requestStream << header.first << ": " << header.second << "\r\n";
        }

        // Add body if present
        if (!body.empty() && headers.find("Content-Length") == headers.end()) {
            requestStream << "Content-Length: " << body.length() << "\r\n";
        }

        // End of headers
        requestStream << "\r\n";

        // Add body
        if (!body.empty()) {
            requestStream << body;
        }

        std::string requestStr = requestStream.str();

        // Send the request
        unified_event::logDebug("Network.HttpClient", "Sending request to " + host + ":" + std::to_string(port));
        bool sendSuccess = false;

        if (scheme == "https") {
            sendSuccess = sslClient->send(reinterpret_cast<const uint8_t*>(requestStr.c_str()), requestStr.length());
            if (!sendSuccess) {
                unified_event::logWarning("Network.HttpClient", "Failed to send HTTPS request to " + host + ":" + std::to_string(port));
                sslClient->disconnect();
                callback(false, {});
                return;
            }

            // Set up the error callback for SSL client
            sslClient->setErrorCallback([&](const std::string& error) {
                unified_event::logWarning("Network.HttpClient", "Error receiving HTTPS response: " + error);
            });

            // Set up the connection closed callback for SSL client
            sslClient->setConnectionClosedCallback([&]() {
                responseComplete = true;
            });
        } else {
            sendSuccess = tcpClient->send(reinterpret_cast<const uint8_t*>(requestStr.c_str()), requestStr.length());
            if (!sendSuccess) {
                unified_event::logWarning("Network.HttpClient", "Failed to send HTTP request to " + host + ":" + std::to_string(port));
                tcpClient->disconnect();
                callback(false, {});
                return;
            }

            // Set up the error callback for TCP client
            tcpClient->setErrorCallback([&](const std::string& error) {
                unified_event::logWarning("Network.HttpClient", "Error receiving HTTP response: " + error);
            });

            // Set up the connection closed callback for TCP client
            tcpClient->setConnectionClosedCallback([&]() {
                responseComplete = true;
            });
        }

        // Set up a timeout
        auto startTime = std::chrono::steady_clock::now();

        // Wait for the response or timeout
        while (!responseComplete) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

            if (elapsed >= m_requestTimeout) {
                unified_event::logWarning("Network.HttpClient", "Request timed out after " + std::to_string(m_requestTimeout) + " seconds");

                if (scheme == "https") {
                    sslClient->disconnect();
                } else {
                    tcpClient->disconnect();
                }

                callback(false, {});
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Disconnect from the server
        if (scheme == "https") {
            sslClient->disconnect();
        } else {
            tcpClient->disconnect();
        }

        // Parse the response
        HttpClientResponse response;
        if (!parseResponse(responseData, response)) {
            unified_event::logWarning("Network.HttpClient", "Failed to parse response");
            callback(false, {});
            return;
        }

        // Check for redirects
        if ((response.statusCode == 301 || response.statusCode == 302 || response.statusCode == 303 ||
             response.statusCode == 307 || response.statusCode == 308) &&
            response.headers.find("location") != response.headers.end()) {

            std::string location = response.headers["location"];
            unified_event::logDebug("Network.HttpClient", "Redirecting to " + location);

            // Handle relative URLs
            if (location.substr(0, 1) == "/") {
                location = scheme + "://" + host + location;
            }

            // Follow the redirect
            get(location, callback);
            return;
        }

        // Call the callback with the response
        callback(true, response);
    }).detach();

    return true;
}

bool HttpClient::parseResponse(const std::string& data, HttpClientResponse& response) {
    // Find the end of the headers
    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        unified_event::logWarning("Network.HttpClient", "Invalid response format - no header/body separator");
        return false;
    }

    // Parse the status line
    size_t statusLineEnd = data.find("\r\n");
    if (statusLineEnd == std::string::npos) {
        unified_event::logWarning("Network.HttpClient", "Invalid response format - no status line");
        return false;
    }

    std::string statusLine = data.substr(0, statusLineEnd);
    std::regex statusRegex("HTTP/\\d\\.\\d (\\d+)");
    std::smatch matches;

    if (!std::regex_search(statusLine, matches, statusRegex)) {
        unified_event::logWarning("Network.HttpClient", "Invalid status line: " + statusLine);
        return false;
    }

    response.statusCode = std::stoi(matches[1].str());

    // Parse the headers
    std::string headersStr = data.substr(statusLineEnd + 2, headerEnd - statusLineEnd - 2);
    std::istringstream headersStream(headersStr);
    std::string line;

    while (std::getline(headersStream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Find the colon
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            unified_event::logWarning("Network.HttpClient", "Invalid header format: " + line);
            continue;
        }

        std::string name = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Convert header name to lowercase for case-insensitive comparison
        std::transform(name.begin(), name.end(), name.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        response.headers[name] = value;
    }

    // Extract the body
    response.body = data.substr(headerEnd + 4);

    return true;
}

} // namespace dht_hunter::network
