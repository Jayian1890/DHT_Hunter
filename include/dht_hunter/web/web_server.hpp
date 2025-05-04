#pragma once

#include "dht_hunter/network/http_server.hpp"
#include "dht_hunter/dht/services/statistics_service.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/bittorrent/metadata/metadata_acquisition_manager.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/web/api/config_api_handler.hpp"
#include "dht_hunter/web/web_bundle_manager.hpp"
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
        std::shared_ptr<dht::PeerStorage> peerStorage,
        std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> metadataManager
    );

    /**
     * @brief Constructor that loads settings from configuration
     * @param statisticsService The statistics service
     * @param routingManager The routing manager
     * @param peerStorage The peer storage
     */
    WebServer(
        std::shared_ptr<dht::services::StatisticsService> statisticsService,
        std::shared_ptr<dht::RoutingManager> routingManager,
        std::shared_ptr<dht::PeerStorage> peerStorage,
        std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> metadataManager
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
    std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> m_metadataManager;
    std::shared_ptr<api::ConfigApiHandler> m_configApiHandler;
    std::shared_ptr<WebBundleManager> m_webBundleManager;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_useBundledWebFiles;

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
     * @brief Handle metadata acquisition API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleMetadataAcquisitionRequest(const network::HttpRequest& request);

    /**
     * @brief Handle metadata acquisition status API request
     * @param request The HTTP request
     * @return The HTTP response
     */
    network::HttpResponse handleMetadataStatusRequest(const network::HttpRequest& request);

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
