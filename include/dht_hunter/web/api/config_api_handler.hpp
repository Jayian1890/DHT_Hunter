#pragma once

#include "dht_hunter/network/http_server.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include <memory>

namespace dht_hunter::web::api {

/**
 * @brief API handler for configuration management
 */
class ConfigApiHandler {
public:
    /**
     * @brief Constructor
     * @param httpServer The HTTP server to register the API endpoints with
     */
    explicit ConfigApiHandler(std::shared_ptr<network::HttpServer> httpServer);

    /**
     * @brief Destructor
     */
    ~ConfigApiHandler();

    /**
     * @brief Registers the API endpoints with the HTTP server
     */
    void registerEndpoints();

private:
    /**
     * @brief Extracts a path parameter from a URL path
     * @param path The URL path
     * @param paramName The parameter name
     * @return The parameter value, or an empty string if not found
     */
    std::string extractPathParam(const std::string& path, const std::string& paramName) const;

    /**
     * @brief Extracts a query parameter from a request
     * @param request The HTTP request
     * @param paramName The parameter name
     * @return The parameter value, or an empty string if not found
     */
    std::string extractQueryParam(const network::HttpRequest& request, const std::string& paramName) const;
    /**
     * @brief Handles GET requests to /api/config
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleGetConfig(const network::HttpRequest& request);

    /**
     * @brief Handles GET requests to /api/config/{key}
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleGetConfigKey(const network::HttpRequest& request);

    /**
     * @brief Handles PUT requests to /api/config/{key}
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handlePutConfigKey(const network::HttpRequest& request);

    /**
     * @brief Handles POST requests to /api/config/reload
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleReloadConfig(const network::HttpRequest& request);

    /**
     * @brief Handles POST requests to /api/config/save
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleSaveConfig(const network::HttpRequest& request);

    // Member variables
    std::shared_ptr<network::HttpServer> m_httpServer;
    std::shared_ptr<utility::config::ConfigurationManager> m_configManager;
    int m_configChangeCallbackId;
};

} // namespace dht_hunter::web::api
