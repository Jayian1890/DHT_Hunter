#include "dht_hunter/web/api/config_api_handler.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/json/json.hpp"

namespace dht_hunter::web::api {

namespace Json = utility::json;

ConfigApiHandler::ConfigApiHandler(std::shared_ptr<network::HttpServer> httpServer)
    : m_httpServer(httpServer),
      m_configManager(utility::config::ConfigurationManager::getInstance()),
      m_configChangeCallbackId(-1) {
    
    // Register the API endpoints
    registerEndpoints();
    
    // Register a callback for configuration changes
    if (m_configManager) {
        m_configChangeCallbackId = m_configManager->registerChangeCallback(
            [this](const std::string& key) {
                unified_event::logInfo("Web.API.ConfigApiHandler", "Configuration changed: " + (key.empty() ? "all" : key));
            }
        );
    }
}

ConfigApiHandler::~ConfigApiHandler() {
    // Unregister the callback
    if (m_configManager && m_configChangeCallbackId >= 0) {
        m_configManager->unregisterChangeCallback(m_configChangeCallbackId);
    }
}

void ConfigApiHandler::registerEndpoints() {
    if (!m_httpServer) {
        unified_event::logError("Web.API.ConfigApiHandler", "HTTP server is null, cannot register endpoints");
        return;
    }
    
    // Register the API endpoints
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/config",
        [this](const network::HttpRequest& request) {
            return handleGetConfig(request);
        }
    );
    
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/api/config/{key}",
        [this](const network::HttpRequest& request) {
            return handleGetConfigKey(request);
        }
    );
    
    m_httpServer->registerRoute(
        network::HttpMethod::PUT,
        "/api/config/{key}",
        [this](const network::HttpRequest& request) {
            return handlePutConfigKey(request);
        }
    );
    
    m_httpServer->registerRoute(
        network::HttpMethod::POST,
        "/api/config/reload",
        [this](const network::HttpRequest& request) {
            return handleReloadConfig(request);
        }
    );
    
    m_httpServer->registerRoute(
        network::HttpMethod::POST,
        "/api/config/save",
        [this](const network::HttpRequest& request) {
            return handleSaveConfig(request);
        }
    );
    
    unified_event::logInfo("Web.API.ConfigApiHandler", "Registered configuration API endpoints");
}

network::HttpResponse ConfigApiHandler::handleGetConfig(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", "Configuration manager is not initialized"}}));
        return response;
    }
    
    try {
        // Get all configuration values as JSON
        std::string configJson = m_configManager->getConfigAsJson(true);
        if (configJson.empty() || configJson == "{}") {
            response.setStatusCode(500);
            response.setBody(Json::stringify({{"error", "Failed to get configuration"}}));
            return response;
        }
        
        // Return the configuration as JSON
        response.setStatusCode(200);
        response.setBody(configJson);
    } catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", std::string("Exception: ") + e.what()}}));
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleGetConfigKey(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", "Configuration manager is not initialized"}}));
        return response;
    }
    
    try {
        // Get the key from the URL
        std::string key = request.getPathParam("key");
        if (key.empty()) {
            response.setStatusCode(400);
            response.setBody(Json::stringify({{"error", "Key is required"}}));
            return response;
        }
        
        // Check if the key exists
        if (!m_configManager->hasKey(key)) {
            response.setStatusCode(404);
            response.setBody(Json::stringify({{"error", "Key not found: " + key}}));
            return response;
        }
        
        // Get the value as JSON
        std::string valueJson = m_configManager->getConfigValueAsJson(key, true);
        if (valueJson.empty()) {
            response.setStatusCode(404);
            response.setBody(Json::stringify({{"error", "Key not found or value is empty: " + key}}));
            return response;
        }
        
        // Return the value as JSON
        response.setStatusCode(200);
        response.setBody(valueJson);
    } catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", std::string("Exception: ") + e.what()}}));
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handlePutConfigKey(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", "Configuration manager is not initialized"}}));
        return response;
    }
    
    try {
        // Get the key from the URL
        std::string key = request.getPathParam("key");
        if (key.empty()) {
            response.setStatusCode(400);
            response.setBody(Json::stringify({{"error", "Key is required"}}));
            return response;
        }
        
        // Get the value from the request body
        std::string body = request.getBody();
        if (body.empty()) {
            response.setStatusCode(400);
            response.setBody(Json::stringify({{"error", "Request body is required"}}));
            return response;
        }
        
        // Set the value
        if (!m_configManager->setConfigValueFromJson(key, body)) {
            response.setStatusCode(500);
            response.setBody(Json::stringify({{"error", "Failed to set value for key: " + key}}));
            return response;
        }
        
        // Return success
        response.setStatusCode(200);
        response.setBody(Json::stringify({{"success", true}, {"key", key}}));
    } catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", std::string("Exception: ") + e.what()}}));
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleReloadConfig(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", "Configuration manager is not initialized"}}));
        return response;
    }
    
    try {
        // Reload the configuration
        if (!m_configManager->reloadConfiguration()) {
            response.setStatusCode(500);
            response.setBody(Json::stringify({{"error", "Failed to reload configuration"}}));
            return response;
        }
        
        // Return success
        response.setStatusCode(200);
        response.setBody(Json::stringify({{"success", true}}));
    } catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", std::string("Exception: ") + e.what()}}));
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleSaveConfig(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", "Configuration manager is not initialized"}}));
        return response;
    }
    
    try {
        // Get the file path from the query parameters
        std::string filePath = request.getQueryParam("path");
        
        // Save the configuration
        bool success;
        if (filePath.empty()) {
            success = m_configManager->saveConfiguration();
        } else {
            success = m_configManager->saveConfiguration(filePath);
        }
        
        if (!success) {
            response.setStatusCode(500);
            response.setBody(Json::stringify({{"error", "Failed to save configuration"}}));
            return response;
        }
        
        // Return success
        response.setStatusCode(200);
        response.setBody(Json::stringify({{"success", true}}));
    } catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setBody(Json::stringify({{"error", std::string("Exception: ") + e.what()}}));
    }
    
    return response;
}

} // namespace dht_hunter::web::api
