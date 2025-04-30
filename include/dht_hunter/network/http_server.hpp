#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

namespace dht_hunter::network {

/**
 * @brief HTTP request method
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    HEAD,
    UNKNOWN
};

/**
 * @brief HTTP request structure
 */
struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;
};

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int statusCode;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Helper method to set content type
    void setContentType(const std::string& contentType) {
        headers["Content-Type"] = contentType;
    }

    // Helper method to set JSON content type
    void setJsonContentType() {
        setContentType("application/json");
    }

    // Helper method to set HTML content type
    void setHtmlContentType() {
        setContentType("text/html");
    }

    // Helper method to set CSS content type
    void setCssContentType() {
        setContentType("text/css");
    }

    // Helper method to set JavaScript content type
    void setJsContentType() {
        setContentType("application/javascript");
    }
};

/**
 * @brief HTTP route handler function type
 */
using HttpRouteHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @class HttpServer
 * @brief A simple HTTP server for serving web content
 */
class HttpServer {
public:
    /**
     * @brief Constructor
     */
    HttpServer();

    /**
     * @brief Destructor
     */
    ~HttpServer();

    /**
     * @brief Start the server on the specified port
     * @param port The port to listen on
     * @return True if successful, false otherwise
     */
    bool start(uint16_t port);

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if the server is running
     * @return True if the server is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Register a route handler for a specific path and method
     * @param method The HTTP method
     * @param path The path to handle
     * @param handler The handler function
     */
    void registerRoute(HttpMethod method, const std::string& path, HttpRouteHandler handler);

    /**
     * @brief Register a static file handler for a specific path
     * @param urlPath The URL path to serve the file from
     * @param filePath The file path on disk
     */
    void registerStaticFile(const std::string& urlPath, const std::string& filePath);

    /**
     * @brief Register a static directory handler
     * @param urlPath The URL path prefix
     * @param dirPath The directory path on disk
     */
    void registerStaticDirectory(const std::string& urlPath, const std::string& dirPath);

private:
    /**
     * @brief Implementation class to hide platform-specific details
     */
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::atomic<bool> m_running;
    std::thread m_serverThread;
    std::mutex m_routeMutex;

    // Route handlers
    std::unordered_map<std::string, std::unordered_map<HttpMethod, HttpRouteHandler>> m_routes;

    // Static file handlers
    std::unordered_map<std::string, std::string> m_staticFiles;

    // Static directory handlers
    std::unordered_map<std::string, std::string> m_staticDirectories;

    /**
     * @brief Server main loop
     */
    void serverLoop(uint16_t port);

    /**
     * @brief Handle an incoming HTTP request
     * @param request The HTTP request
     * @return The HTTP response
     */
    HttpResponse handleRequest(const HttpRequest& request);

    /**
     * @brief Serve a static file
     * @param filePath The path to the file
     * @return The HTTP response
     */
    HttpResponse serveStaticFile(const std::string& filePath);

    /**
     * @brief Parse query parameters from a URL
     * @param url The URL to parse
     * @return A map of query parameters
     */
    std::unordered_map<std::string, std::string> parseQueryParams(const std::string& url);

    /**
     * @brief Get the content type for a file extension
     * @param extension The file extension
     * @return The content type
     */
    std::string getContentTypeForExtension(const std::string& extension);
};

} // namespace dht_hunter::network
