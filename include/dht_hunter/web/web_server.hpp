#pragma once

#include "dht_hunter/network/http_server.hpp"
#include "dht_hunter/dht/services/statistics_service.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include <memory>
#include <string>
#include <chrono>

namespace dht_hunter::web {

/**
 * @class WebServer
 * @brief Web server for the DHT Hunter application
 *
 * This class provides a web interface for the DHT Hunter application,
 * exposing statistics and other information via a web UI.
 */
class WebServer {
public:
    /**
     * @brief Constructor
     * @param webRoot The root directory for web files
     * @param port The port to listen on
     * @param statisticsService The statistics service
     * @param routingManager The routing manager
     * @param peerStorage The peer storage
     */
    WebServer(
        const std::string& webRoot,
        uint16_t port,
        std::shared_ptr<dht::services::StatisticsService> statisticsService,
        std::shared_ptr<dht::RoutingManager> routingManager,
        std::shared_ptr<dht::PeerStorage> peerStorage
    );

    /**
     * @brief Destructor
     */
    ~WebServer();

    /**
     * @brief Start the web server
     * @return True if successful, false otherwise
     */
    bool start();

    /**
     * @brief Stop the web server
     */
    void stop();

    /**
     * @brief Check if the web server is running
     * @return True if the web server is running, false otherwise
     */
    bool isRunning() const;

private:
    std::string m_webRoot;
    uint16_t m_port;
    std::shared_ptr<network::HttpServer> m_httpServer;
    std::shared_ptr<dht::services::StatisticsService> m_statisticsService;
    std::shared_ptr<dht::RoutingManager> m_routingManager;
    std::shared_ptr<dht::PeerStorage> m_peerStorage;
    std::shared_ptr<types::InfoHashMetadataRegistry> m_metadataRegistry;
    std::chrono::steady_clock::time_point m_startTime;

    /**
     * @brief Register API routes
     */
    void registerApiRoutes();

    /**
     * @brief Register static file routes
     */
    void registerStaticRoutes();

    /**
     * @brief Handle statistics API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleStatisticsRequest(const network::HttpRequest& request);

    /**
     * @brief Handle nodes API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleNodesRequest(const network::HttpRequest& request);

    /**
     * @brief Handle info hashes API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleInfoHashesRequest(const network::HttpRequest& request);

    /**
     * @brief Handle uptime API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleUptimeRequest(const network::HttpRequest& request);

    /**
     * @brief Serve a static file
     * @param filePath The path to the file
     * @return The HTTP response
     */
    network::HttpResponse serveStaticFile(const std::string& filePath);
};

} // namespace dht_hunter::web
