#include "dht_hunter/network/http_server.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/network/network_utils.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    // Define a function instead of a macro for closesocket
    inline int closesocket(int s) { return ::close(s); }
#endif

namespace dht_hunter::network {

// Helper function to convert string to HttpMethod
HttpMethod stringToHttpMethod(const std::string& method) {
    if (method == "GET") return HttpMethod::GET;
    if (method == "POST") return HttpMethod::POST;
    if (method == "PUT") return HttpMethod::PUT;
    if (method == "DELETE") return HttpMethod::DELETE;
    if (method == "OPTIONS") return HttpMethod::OPTIONS;
    if (method == "HEAD") return HttpMethod::HEAD;
    return HttpMethod::UNKNOWN;
}

// Helper function to convert HttpMethod to string
std::string httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::HEAD: return "HEAD";
        default: return "UNKNOWN";
    }
}

// Implementation class
class HttpServer::Impl {
public:
    Impl() : m_socket(INVALID_SOCKET) {
        // Initialize socket library on Windows
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            unified_event::logError("Network.HttpServer", "Failed to initialize Winsock");
        }
#endif
    }

    ~Impl() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
        }

#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool createSocket() {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            unified_event::logError("Network.HttpServer", "Failed to create socket");
            return false;
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
            unified_event::logError("Network.HttpServer", "Failed to set socket options");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        return true;
    }

    bool bindAndListen(uint16_t port) {
        if (m_socket == INVALID_SOCKET) {
            return false;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            unified_event::logError("Network.HttpServer", "Failed to bind socket");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
            unified_event::logError("Network.HttpServer", "Failed to listen on socket");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        return true;
    }

    SOCKET acceptConnection() {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(m_socket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            unified_event::logError("Network.HttpServer", "Failed to accept connection");
            return INVALID_SOCKET;
        }

        return clientSocket;
    }

    HttpRequest parseRequest(SOCKET clientSocket) {
        HttpRequest request;
        std::vector<char> buffer(4096);

        // Receive data
        ssize_t bytesReceived = recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
        if (bytesReceived <= 0) {
            return request;
        }

        buffer[static_cast<size_t>(bytesReceived)] = '\0';
        std::string requestStr(buffer.data(), static_cast<size_t>(bytesReceived));

        // Parse request line
        size_t pos = requestStr.find("\r\n");
        if (pos == std::string::npos) {
            return request;
        }

        std::string requestLine = requestStr.substr(0, pos);
        std::istringstream requestLineStream(requestLine);
        std::string methodStr, path, httpVersion;
        requestLineStream >> methodStr >> path >> httpVersion;

        request.method = stringToHttpMethod(methodStr);

        // Parse path and query parameters
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            request.path = path.substr(0, queryPos);
            std::string queryString = path.substr(queryPos + 1);
            request.queryParams = parseQueryParams(queryString);
        } else {
            request.path = path;
        }

        // Parse headers
        size_t headerStart = pos + 2;
        size_t headerEnd = requestStr.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return request;
        }

        std::string headersStr = requestStr.substr(headerStart, headerEnd - headerStart);
        std::istringstream headersStream(headersStr);
        std::string headerLine;

        while (std::getline(headersStream, headerLine)) {
            if (headerLine.empty() || headerLine == "\r") {
                continue;
            }

            // Remove trailing \r if present
            if (!headerLine.empty() && headerLine.back() == '\r') {
                headerLine.pop_back();
            }

            size_t colonPos = headerLine.find(':');
            if (colonPos != std::string::npos) {
                std::string name = headerLine.substr(0, colonPos);
                std::string value = headerLine.substr(colonPos + 1);

                // Trim leading and trailing whitespace
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                request.headers[name] = value;
            }
        }

        // Parse body
        if (headerEnd + 4 < requestStr.size()) {
            request.body = requestStr.substr(headerEnd + 4);
        }

        return request;
    }

    void sendResponse(SOCKET clientSocket, const HttpResponse& response) {
        std::ostringstream responseStream;

        // Status line
        responseStream << "HTTP/1.1 " << response.statusCode << " ";
        switch (response.statusCode) {
            case 200: responseStream << "OK"; break;
            case 201: responseStream << "Created"; break;
            case 204: responseStream << "No Content"; break;
            case 302: responseStream << "Found"; break;
            case 304: responseStream << "Not Modified"; break;
            case 400: responseStream << "Bad Request"; break;
            case 401: responseStream << "Unauthorized"; break;
            case 403: responseStream << "Forbidden"; break;
            case 404: responseStream << "Not Found"; break;
            case 500: responseStream << "Internal Server Error"; break;
            default: responseStream << "Unknown";
        }
        responseStream << "\r\n";

        // Headers
        for (const auto& [name, value] : response.headers) {
            responseStream << name << ": " << value << "\r\n";
        }

        // Add server header
        responseStream << "Server: " << utility::network::getUserAgent() << "\r\n";

        // Add date header
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::gmtime(&now_time_t);
        char date_buf[100];
        std::strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &now_tm);
        responseStream << "Date: " << date_buf << "\r\n";

        // Content-Length header
        responseStream << "Content-Length: " << response.body.size() << "\r\n";

        // Add Content-Type header if not present
        if (response.headers.find("Content-Type") == response.headers.end()) {
            responseStream << "Content-Type: text/plain; charset=utf-8\r\n";
        }

        // End of headers
        responseStream << "\r\n";

        // Body
        responseStream << response.body;

        std::string responseStr = responseStream.str();

        // Log response info
        unified_event::logDebug("Network.HttpServer", "Sending response: " + std::to_string(response.statusCode) +
                              ", Content-Type: " + (response.headers.find("Content-Type") != response.headers.end() ?
                                                 response.headers.at("Content-Type") : "text/plain") +
                              ", Content-Length: " + std::to_string(response.body.size()));

        // Send the response
        ssize_t bytesSent = send(clientSocket, responseStr.c_str(), responseStr.size(), 0);
        if (bytesSent < 0) {
            unified_event::logError("Network.HttpServer", "Failed to send response: " + std::to_string(errno));
        } else if (static_cast<size_t>(bytesSent) < responseStr.size()) {
            unified_event::logWarning("Network.HttpServer", "Incomplete response sent: " +
                                    std::to_string(bytesSent) + "/" + std::to_string(responseStr.size()) + " bytes");
        }
    }

    std::unordered_map<std::string, std::string> parseQueryParams(const std::string& queryString) {
        std::unordered_map<std::string, std::string> params;
        std::istringstream queryStream(queryString);
        std::string param;

        while (std::getline(queryStream, param, '&')) {
            size_t equalsPos = param.find('=');
            if (equalsPos != std::string::npos) {
                std::string name = param.substr(0, equalsPos);
                std::string value = param.substr(equalsPos + 1);
                params[name] = value;
            } else {
                params[param] = "";
            }
        }

        return params;
    }

    SOCKET m_socket;
};

// HttpServer implementation
HttpServer::HttpServer() : m_impl(std::make_unique<Impl>()), m_running(false) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(uint16_t port) {
    if (m_running) {
        return true;
    }

    unified_event::logInfo("Network.HttpServer", "Starting HTTP server on port " + std::to_string(port));

    m_running = true;
    m_serverThread = std::thread(&HttpServer::serverLoop, this, port);

    return true;
}

void HttpServer::stop() {
    if (!m_running) {
        return;
    }

    unified_event::logInfo("Network.HttpServer", "Stopping HTTP server");

    m_running = false;

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

bool HttpServer::isRunning() const {
    return m_running;
}

void HttpServer::registerRoute(HttpMethod method, const std::string& path, HttpRouteHandler handler) {
    std::lock_guard<std::mutex> lock(m_routeMutex);
    m_routes[path][method] = handler;
    unified_event::logDebug("Network.HttpServer", "Registered route: " + httpMethodToString(method) + " " + path);
}

void HttpServer::registerStaticFile(const std::string& urlPath, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_routeMutex);
    m_staticFiles[urlPath] = filePath;
    unified_event::logDebug("Network.HttpServer", "Registered static file: " + urlPath + " -> " + filePath);
}

void HttpServer::registerStaticDirectory(const std::string& urlPath, const std::string& dirPath) {
    std::lock_guard<std::mutex> lock(m_routeMutex);
    m_staticDirectories[urlPath] = dirPath;
    unified_event::logDebug("Network.HttpServer", "Registered static directory: " + urlPath + " -> " + dirPath);
}

void HttpServer::serverLoop(uint16_t port) {
    if (!m_impl->createSocket()) {
        unified_event::logError("Network.HttpServer", "Failed to create socket");
        m_running = false;
        return;
    }

    if (!m_impl->bindAndListen(port)) {
        unified_event::logError("Network.HttpServer", "Failed to bind and listen on port " + std::to_string(port));
        m_running = false;
        return;
    }

    unified_event::logInfo("Network.HttpServer", "HTTP server listening on port " + std::to_string(port));

    while (m_running) {
        SOCKET clientSocket = m_impl->acceptConnection();
        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        // Handle the request in a separate thread
        std::thread([this, clientSocket]() {
            HttpRequest request = m_impl->parseRequest(clientSocket);
            HttpResponse response = handleRequest(request);
            m_impl->sendResponse(clientSocket, response);
            closesocket(clientSocket);
        }).detach();
    }
}

HttpResponse HttpServer::handleRequest(const HttpRequest& request) {
    unified_event::logDebug("Network.HttpServer", "Handling request: " + httpMethodToString(request.method) + " " + request.path);

    // Check for static file handlers
    {
        std::lock_guard<std::mutex> lock(m_routeMutex);
        auto it = m_staticFiles.find(request.path);
        if (it != m_staticFiles.end()) {
            return serveStaticFile(it->second);
        }
    }

    // Check for static directory handlers
    {
        std::lock_guard<std::mutex> lock(m_routeMutex);
        for (const auto& [urlPath, dirPath] : m_staticDirectories) {
            if (request.path.starts_with(urlPath)) {
                std::string relativePath = request.path.substr(urlPath.size());
                if (relativePath.empty() || relativePath[0] != '/') {
                    relativePath = "/" + relativePath;
                }

                // If the path is just the directory, serve index.html
                if (relativePath == "/") {
                    relativePath = "/index.html";
                }

                std::string fullPath = dirPath + relativePath;
                unified_event::logDebug("Network.HttpServer", "Serving file from directory: " + fullPath);

                // Check if the file exists
                if (!std::filesystem::exists(fullPath)) {
                    unified_event::logWarning("Network.HttpServer", "File not found in static directory: " + fullPath);
                    continue; // Try next directory or fall back to route handlers
                }

                // Check if it's a directory
                if (std::filesystem::is_directory(fullPath)) {
                    // Try to serve index.html from the directory
                    std::string indexPath = fullPath + "/index.html";
                    if (std::filesystem::exists(indexPath)) {
                        unified_event::logDebug("Network.HttpServer", "Serving index.html from directory: " + indexPath);
                        return serveStaticFile(indexPath);
                    }

                    unified_event::logWarning("Network.HttpServer", "Directory does not contain index.html: " + fullPath);
                    continue; // Try next directory or fall back to route handlers
                }

                return serveStaticFile(fullPath);
            }
        }
    }

    // Check for route handlers
    {
        std::lock_guard<std::mutex> lock(m_routeMutex);
        auto routeIt = m_routes.find(request.path);
        if (routeIt != m_routes.end()) {
            auto methodIt = routeIt->second.find(request.method);
            if (methodIt != routeIt->second.end()) {
                try {
                    return methodIt->second(request);
                } catch (const std::exception& e) {
                    unified_event::logError("Network.HttpServer", "Error handling request: " + std::string(e.what()));
                    HttpResponse response;
                    response.statusCode = 500;
                    response.body = "Internal Server Error";
                    return response;
                }
            }
        }
    }

    // Not found
    HttpResponse response;
    response.statusCode = 404;
    response.body = "Not Found";
    return response;
}

HttpResponse HttpServer::serveStaticFile(const std::string& filePath) {
    HttpResponse response;

    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        unified_event::logError("Network.HttpServer", "File not found: " + filePath);
        response.statusCode = 404;
        response.body = "File not found";
        response.setContentType("text/plain");
        return response;
    }

    // Open file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        unified_event::logError("Network.HttpServer", "Failed to open file: " + filePath);
        response.statusCode = 500;
        response.body = "Failed to open file";
        response.setContentType("text/plain");
        return response;
    }

    // Read file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    response.body = buffer.str();

    // Set content type based on file extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::string contentType = getContentTypeForExtension(extension);
    response.setContentType(contentType);

    unified_event::logDebug("Network.HttpServer", "Serving file: " + filePath + " with content type: " + contentType);

    response.statusCode = 200;
    return response;
}

std::string HttpServer::getContentTypeForExtension(const std::string& extension) {
    std::string ext = extension;
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".xml") return "application/xml; charset=utf-8";
    if (ext == ".pdf") return "application/pdf";

    unified_event::logWarning("Network.HttpServer", "Unknown file extension: " + ext + ", using default content type");
    return "application/octet-stream";
}

std::unordered_map<std::string, std::string> HttpServer::parseQueryParams(const std::string& url) {
    size_t queryPos = url.find('?');
    if (queryPos == std::string::npos) {
        return {};
    }

    std::string queryString = url.substr(queryPos + 1);
    return m_impl->parseQueryParams(queryString);
}

} // namespace dht_hunter::network
