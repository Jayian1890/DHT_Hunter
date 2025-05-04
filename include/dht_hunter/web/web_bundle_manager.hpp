#pragma once

#include "dht_hunter/web/bundle/web_bundle.hpp"
#include "dht_hunter/network/http_server.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace dht_hunter::web {

/**
 * @class WebBundleManager
 * @brief Manages bundled web files
 *
 * This class provides access to web files that are bundled with the executable.
 * It registers routes with the HTTP server to serve the bundled files.
 */
class WebBundleManager {
public:
    /**
     * @brief Constructor
     * @param httpServer The HTTP server to register routes with
     */
    WebBundleManager(std::shared_ptr<network::HttpServer> httpServer);

    /**
     * @brief Destructor
     */
    ~WebBundleManager() = default;

    /**
     * @brief Register routes for bundled web files
     */
    void registerRoutes();

    /**
     * @brief Get a bundled file by path
     * @param path The path to the file
     * @return The file content, or nullptr if not found
     */
    const std::vector<unsigned char>* getFile(const std::string& path) const;

    /**
     * @brief Get a bundled file MIME type by path
     * @param path The path to the file
     * @return The MIME type
     */
    std::string getMimeType(const std::string& path) const;

    /**
     * @brief Check if a file is bundled
     * @param path The path to the file
     * @return True if the file is bundled, false otherwise
     */
    bool isFileBundled(const std::string& path) const;

    /**
     * @brief Get all bundled file paths
     * @return A map of file paths
     */
    std::unordered_map<std::string, std::string> getAllFilePaths() const;

private:
    std::shared_ptr<network::HttpServer> m_httpServer;
    std::unordered_map<std::string, const std::vector<unsigned char>*> m_files;
    std::unordered_map<std::string, std::string> m_filePaths;
    std::unordered_map<std::string, std::string> m_mimeTypes;

    /**
     * @brief Handle a request for a bundled file
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleBundledFileRequest(const network::HttpRequest& request);
};

} // namespace dht_hunter::web
