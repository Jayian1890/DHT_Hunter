#include "dht_hunter/web/web_server.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/json/json.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace dht_hunter::web {

using Json = utility::json::Json;
using JsonValue = utility::json::JsonValue;

WebServer::WebServer(const std::string& webRoot, uint16_t port, std::shared_ptr<dht::services::StatisticsService> statisticsService, std::shared_ptr<dht::RoutingManager> routingManager, std::shared_ptr<dht::PeerStorage> peerStorage,
                     std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> metadataManager)
    : m_webRoot(webRoot),
      m_port(port),
      m_httpServer(std::make_shared<network::HttpServer>()),
      m_statisticsService(statisticsService),
      m_routingManager(routingManager),
      m_peerStorage(peerStorage),
      m_metadataRegistry(types::InfoHashMetadataRegistry::getInstance()),
      m_metadataManager(metadataManager),
      m_startTime(std::chrono::steady_clock::now()) {
    unified_event::logDebug("Web.Server", "WebServer initialized with webRoot: " + webRoot + ", port: " + std::to_string(port));
}

WebServer::WebServer(std::shared_ptr<dht::services::StatisticsService> statisticsService, std::shared_ptr<dht::RoutingManager> routingManager, std::shared_ptr<dht::PeerStorage> peerStorage, std::shared_ptr<bittorrent::metadata::MetadataAcquisitionManager> metadataManager)
    : m_httpServer(std::make_shared<network::HttpServer>()),
      m_statisticsService(statisticsService),
      m_routingManager(routingManager),
      m_peerStorage(peerStorage),
      m_metadataRegistry(types::InfoHashMetadataRegistry::getInstance()),
      m_metadataManager(metadataManager),
      m_startTime(std::chrono::steady_clock::now()) {
    // Get settings from configuration
    auto configManager = utility::config::ConfigurationManager::getInstance();
    m_webRoot = configManager->getString("web.webRoot", "web");
    m_port = static_cast<uint16_t>(configManager->getInt("web.port", 8080));

    unified_event::logDebug("Web.Server", "WebServer initialized from configuration with webRoot: " + m_webRoot + ", port: " + std::to_string(m_port));
}
}

using dht_hunter::web::WebServer;

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (m_httpServer->isRunning()) {
        return true;
    }

    unified_event::logDebug("Web.Server", "Starting web server on port " + std::to_string(m_port));

    // Register routes
    registerApiRoutes();
    registerStaticRoutes();

    // Start the HTTP server
    return m_httpServer->start(m_port);
}

void WebServer::stop() {
    if (!m_httpServer->isRunning()) {
        return;
    }

    unified_event::logInfo("Web.Server", "Stopping web server");
    m_httpServer->stop();
}

bool WebServer::isRunning() const {
    return m_httpServer->isRunning();
}

void WebServer::registerApiRoutes() {
    // Statistics API
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/statistics",
        [this](const network::HttpRequest& request) {
            return handleStatisticsRequest(request);
        }
    );

    // Nodes API
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/nodes",
        [this](const network::HttpRequest& request) {
            return handleNodesRequest(request);
        }
    );

    // Info hashes API
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/infohashes",
        [this](const network::HttpRequest& request) {
            return handleInfoHashesRequest(request);
        }
    );

    // Uptime API
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/uptime",
        [this](const network::HttpRequest& request) {
            return handleUptimeRequest(request);
        }
    );

    // Metadata acquisition API
    m_httpServer->registerRoute(
        network::HttpMethod::POST,
        "/api/metadata/acquire",
        [this](const network::HttpRequest& request) {
            return handleMetadataAcquisitionRequest(request);
        }
    );

    // Metadata status API
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/metadata/status",
        [this](const network::HttpRequest& request) {
            return handleMetadataStatusRequest(request);
        }
    );

    // Initialize the configuration API handler
    m_configApiHandler = std::make_shared<api::ConfigApiHandler>(m_httpServer);
}

void WebServer::registerStaticRoutes() {
    // Check if web root exists
    if (!std::filesystem::exists(m_webRoot)) {
        unified_event::logError("Web.Server", "Web root directory does not exist: " + m_webRoot);
        return;
    }

    unified_event::logInfo("Web.Server", "Registering static files from directory: " + m_webRoot);

    // List files in web root to verify
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_webRoot)) {
            unified_event::logInfo("Web.Server", "Found file: " + entry.path().string());
        }
    } catch (const std::exception& e) {
        unified_event::logError("Web.Server", "Error listing web root directory: " + std::string(e.what()));
    }

    // Register static directory
    m_httpServer->registerStaticDirectory("/", m_webRoot);

    // Register specific static files explicitly
    m_httpServer->registerStaticFile("/index.html", m_webRoot + "/index.html");
    m_httpServer->registerStaticFile("/css/styles.css", m_webRoot + "/css/styles.css");
    m_httpServer->registerStaticFile("/js/dashboard.js", m_webRoot + "/js/dashboard.js");

    // Register index.html for root path
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/",
        [this](const network::HttpRequest& /*request*/) {
            unified_event::logInfo("Web.Server", "Serving index.html for root path");

            // Check if index.html exists
            std::string indexPath = m_webRoot + "/index.html";
            if (!std::filesystem::exists(indexPath)) {
                unified_event::logError("Web.Server", "index.html not found at: " + indexPath);
                network::HttpResponse response;
                response.statusCode = 404;
                response.body = "index.html not found";
                response.setContentType("text/plain");
                return response;
            }

            // Read the file content
            std::ifstream file(indexPath, std::ios::binary);
            if (!file) {
                unified_event::logError("Web.Server", "Failed to open index.html: " + indexPath);
                network::HttpResponse response;
                response.statusCode = 500;
                response.body = "Failed to open index.html";
                response.setContentType("text/plain");
                return response;
            }

            // Create the response
            network::HttpResponse response;
            std::stringstream buffer;
            buffer << file.rdbuf();
            response.body = buffer.str();
            response.setHtmlContentType();
            response.statusCode = 200;

            unified_event::logInfo("Web.Server", "Successfully served index.html for root path, size: " +
                                 std::to_string(response.body.size()) + " bytes");

            return response;
        }
    );
}

dht_hunter::network::HttpResponse WebServer::handleStatisticsRequest(const dht_hunter::network::HttpRequest& /*request*/) {
    dht_hunter::network::HttpResponse response;
    response.statusCode = 200;
    response.setJsonContentType();

    auto statsObj = Json::createObject();

    // Get statistics from services
    if (m_statisticsService) {
        statsObj->set("nodesDiscovered", static_cast<long long>(m_statisticsService->getNodesDiscovered()));
        statsObj->set("nodesAdded", static_cast<long long>(m_statisticsService->getNodesAdded()));
        statsObj->set("peersDiscovered", static_cast<long long>(m_statisticsService->getPeersDiscovered()));
        statsObj->set("messagesReceived", static_cast<long long>(m_statisticsService->getMessagesReceived()));
        statsObj->set("messagesSent", static_cast<long long>(m_statisticsService->getMessagesSent()));
        statsObj->set("errors", static_cast<long long>(m_statisticsService->getErrors()));
    }

    // Get routing table statistics
    if (m_routingManager) {
        statsObj->set("nodesInTable", static_cast<long long>(m_routingManager->getNodeCount()));
    } else {
        statsObj->set("nodesInTable", 0);
    }

    // Get peer storage statistics
    if (m_peerStorage) {
        statsObj->set("infoHashes", static_cast<long long>(m_peerStorage->getInfoHashCount()));
    } else {
        statsObj->set("infoHashes", 0);
    }

    response.body = Json::stringify(JsonValue(statsObj));
    return response;
}

dht_hunter::network::HttpResponse WebServer::handleNodesRequest(const dht_hunter::network::HttpRequest& request) {
    dht_hunter::network::HttpResponse response;
    response.statusCode = 200;
    response.setJsonContentType();

    // Parse limit parameter
    size_t limit = 10;
    auto it = request.queryParams.find("limit");
    if (it != request.queryParams.end()) {
        try {
            limit = std::stoul(it->second);
        } catch (const std::exception& e) {
            unified_event::logWarning("Web.Server", "Invalid limit parameter: " + it->second);
        }
    }

    auto nodesArray = Json::createArray();

    // Get nodes from routing table
    if (m_routingManager) {
        auto routingTable = m_routingManager->getRoutingTable();
        if (routingTable) {
            auto nodes = routingTable->getAllNodes();

            // Sort nodes by last seen time (most recent first)
            std::sort(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
                return a->getLastSeen() > b->getLastSeen();
            });

            // Limit the number of nodes
            size_t count = std::min(limit, nodes.size());

            for (size_t i = 0; i < count; ++i) {
                const auto& node = nodes[i];
                auto nodeObj = Json::createObject();
                nodeObj->set("nodeId", node->getID().toString());
                nodeObj->set("ip", node->getEndpoint().getAddress().toString());
                nodeObj->set("port", static_cast<int>(node->getEndpoint().getPort()));
                nodeObj->set("lastSeen", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    node->getLastSeen().time_since_epoch()
                ).count()));
                nodesArray->add(JsonValue(nodeObj));
            }
        }
    }

    response.body = Json::stringify(JsonValue(nodesArray));
    return response;
}

dht_hunter::network::HttpResponse WebServer::handleInfoHashesRequest(const dht_hunter::network::HttpRequest& request) {
    dht_hunter::network::HttpResponse response;
    response.statusCode = 200;
    response.setJsonContentType();

    // Parse limit parameter
    size_t limit = 10;
    auto it = request.queryParams.find("limit");
    if (it != request.queryParams.end()) {
        try {
            limit = std::stoul(it->second);
        } catch (const std::exception& e) {
            unified_event::logWarning("Web.Server", "Invalid limit parameter: " + it->second);
        }
    }

    auto infoHashesArray = Json::createArray();

    // Get info hashes from peer storage
    if (m_peerStorage) {
        auto infoHashes = m_peerStorage->getAllInfoHashes();

        // Limit the number of info hashes
        size_t count = std::min(limit, infoHashes.size());

        for (size_t i = 0; i < count; ++i) {
            const auto& infoHash = infoHashes[i];
            auto peers = m_peerStorage->getPeers(infoHash);

            auto infoHashObj = Json::createObject();
            std::string infoHashStr = dht_hunter::types::infoHashToString(infoHash);
            infoHashObj->set("hash", infoHashStr);
            infoHashObj->set("peers", static_cast<long long>(peers.size()));

            // Use current time as a placeholder for first seen
            // In a real implementation, you would store this information
            infoHashObj->set("firstSeen", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()));

            // Add metadata if available
            if (m_metadataRegistry) {
                auto metadata = m_metadataRegistry->getMetadata(infoHash);
                if (metadata) {
                    // Add name if available
                    const std::string& name = metadata->getName();
                    if (!name.empty()) {
                        infoHashObj->set("name", name);
                    }

                    // Add total size if available
                    uint64_t totalSize = metadata->getTotalSize();
                    if (totalSize > 0) {
                        infoHashObj->set("size", static_cast<long long>(totalSize));
                    }

                    // Add file count if available
                    const auto& files = metadata->getFiles();
                    if (!files.empty()) {
                        infoHashObj->set("fileCount", static_cast<long long>(files.size()));
                    }
                } else {
                    // Try to get metadata using utility
                    std::string name = utility::metadata::MetadataUtils::getInfoHashName(infoHash);
                    if (!name.empty()) {
                        infoHashObj->set("name", name);
                    }

                    uint64_t totalSize = utility::metadata::MetadataUtils::getInfoHashTotalSize(infoHash);
                    if (totalSize > 0) {
                        infoHashObj->set("size", static_cast<long long>(totalSize));
                    }

                    auto files = utility::metadata::MetadataUtils::getInfoHashFiles(infoHash);
                    if (!files.empty()) {
                        infoHashObj->set("fileCount", static_cast<long long>(files.size()));
                    }
                }
            }

            // Add metadata acquisition status if available
            if (m_metadataManager) {
                std::string status = m_metadataManager->getAcquisitionStatus(infoHash);
                if (!status.empty()) {
                    infoHashObj->set("metadataStatus", status);
                }
            }

            infoHashesArray->add(JsonValue(infoHashObj));
        }
    }

    response.body = Json::stringify(JsonValue(infoHashesArray));
    return response;
}

dht_hunter::network::HttpResponse WebServer::handleUptimeRequest(const dht_hunter::network::HttpRequest& /*request*/) {
    dht_hunter::network::HttpResponse response;
    response.statusCode = 200;
    response.setJsonContentType();

    auto uptimeObj = Json::createObject();

    // Calculate uptime in seconds
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();

    uptimeObj->set("uptime", static_cast<long long>(uptime));

    response.body = Json::stringify(JsonValue(uptimeObj));
    return response;
}

dht_hunter::network::HttpResponse WebServer::serveStaticFile(const std::string& filePath) {
    unified_event::logInfo("Web.Server", "Serving static file: " + filePath);
    dht_hunter::network::HttpResponse response;

    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
        unified_event::logError("Web.Server", "File not found: " + filePath);
        response.statusCode = 404;
        response.body = "File not found";
        response.setContentType("text/plain");
        return response;
    }

    // Open file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        unified_event::logError("Web.Server", "Failed to open file: " + filePath);
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
    if (extension == ".html" || extension == ".htm") {
        response.setHtmlContentType();
    } else if (extension == ".css") {
        response.setCssContentType();
    } else if (extension == ".js") {
        response.setJsContentType();
    } else {
        response.setContentType("application/octet-stream");
    }

    response.statusCode = 200;
    return response;
}

dht_hunter::network::HttpResponse WebServer::handleMetadataAcquisitionRequest(const dht_hunter::network::HttpRequest& request) {
    dht_hunter::network::HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    // Parse the request body as JSON
    try {
        auto json = utility::json::JsonValue::parse(request.body);

        // Get the info hash from the request
        if (!json.getObject()->has("infoHash") || !json.getObject()->get("infoHash").isString()) {
            response.statusCode = 400;

            auto errorObj = utility::json::JsonValue::createObject();
            errorObj->set("success", utility::json::JsonValue(false));
            errorObj->set("error", utility::json::JsonValue("Missing or invalid infoHash parameter"));

            response.body = utility::json::JsonValue(errorObj).toString();
            return response;
        }

        std::string infoHashStr = json.getObject()->get("infoHash").getString();
        types::InfoHash infoHash;

        if (!types::infoHashFromString(infoHashStr, infoHash)) {
            response.statusCode = 400;

            auto errorObj = utility::json::JsonValue::createObject();
            errorObj->set("success", utility::json::JsonValue(false));
            errorObj->set("error", utility::json::JsonValue("Invalid info hash format"));

            response.body = utility::json::JsonValue(errorObj).toString();
            return response;
        }

        // Get the priority from the request (optional)
        int priority = 0;
        if (json.getObject()->has("priority") && json.getObject()->get("priority").isNumber()) {
            priority = json.getObject()->get("priority").getInt();
        }

        // Trigger metadata acquisition
        if (!m_metadataManager || !m_metadataManager->acquireMetadata(infoHash, priority)) {
            response.statusCode = 500;

            auto errorObj = utility::json::JsonValue::createObject();
            errorObj->set("success", utility::json::JsonValue(false));
            errorObj->set("error", utility::json::JsonValue("Failed to start metadata acquisition"));

            response.body = utility::json::JsonValue(errorObj).toString();
            return response;
        }

        // Return success response
        response.statusCode = 200;

        auto successObj = utility::json::JsonValue::createObject();
        successObj->set("success", utility::json::JsonValue(true));
        successObj->set("infoHash", utility::json::JsonValue(infoHashStr));
        successObj->set("message", utility::json::JsonValue("Metadata acquisition started"));

        response.body = utility::json::JsonValue(successObj).toString();

    } catch (const std::exception& e) {
        response.statusCode = 400;

        auto errorObj = utility::json::JsonValue::createObject();
        errorObj->set("success", utility::json::JsonValue(false));
        errorObj->set("error", utility::json::JsonValue(std::string("Error parsing request: ") + e.what()));

        response.body = utility::json::JsonValue(errorObj).toString();
    }

    return response;
}

dht_hunter::network::HttpResponse WebServer::handleMetadataStatusRequest(const dht_hunter::network::HttpRequest& request) {
    dht_hunter::network::HttpResponse response;
    response.headers["Content-Type"] = "application/json";

    if (!m_metadataManager) {
        response.statusCode = 500;

        auto errorObj = utility::json::JsonValue::createObject();
        errorObj->set("success", utility::json::JsonValue(false));
        errorObj->set("error", utility::json::JsonValue("Metadata acquisition manager not available"));

        response.body = utility::json::JsonValue(errorObj).toString();
        return response;
    }

    // Get the info hash from the query parameters (optional)
    auto params = request.queryParams;
    auto it = params.find("infoHash");

    if (it != params.end()) {
        // Get status for a specific info hash
        std::string infoHashStr = it->second;
        types::InfoHash infoHash;

        if (!types::infoHashFromString(infoHashStr, infoHash)) {
            response.statusCode = 400;

            auto errorObj = utility::json::JsonValue::createObject();
            errorObj->set("success", utility::json::JsonValue(false));
            errorObj->set("error", utility::json::JsonValue("Invalid info hash format"));

            response.body = utility::json::JsonValue(errorObj).toString();
            return response;
        }

        std::string status = m_metadataManager->getAcquisitionStatus(infoHash);

        response.statusCode = 200;

        auto successObj = utility::json::JsonValue::createObject();
        successObj->set("success", utility::json::JsonValue(true));
        successObj->set("infoHash", utility::json::JsonValue(infoHashStr));
        successObj->set("status", utility::json::JsonValue(status));

        response.body = utility::json::JsonValue(successObj).toString();
    } else {
        // Get overall statistics
        auto stats = m_metadataManager->getStatistics();

        auto statsObj = utility::json::JsonValue::createObject();
        for (const auto& [key, value] : stats) {
            statsObj->set(key, utility::json::JsonValue(static_cast<double>(value)));
        }

        response.statusCode = 200;

        auto successObj = utility::json::JsonValue::createObject();
        successObj->set("success", utility::json::JsonValue(true));
        successObj->set("statistics", utility::json::JsonValue(statsObj));

        response.body = utility::json::JsonValue(successObj).toString();
    }

    return response;
} // namespace dht_hunter::web
