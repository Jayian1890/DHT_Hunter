#include "dht_hunter/web/api/config_api_handler.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/json/json.hpp"

namespace dht_hunter::web::api {

namespace json = utility::json;

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
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", "Configuration manager is not initialized"}});
        return response;
    }
    
    try {
        // Get all configuration values as JSON
        std::string configJson = m_configManager->getConfigAsJson(true);
        if (configJson.empty() || configJson == "{}") {
            response.statusCode = 500;
            response.body = json::Json::stringify({{"error", "Failed to get configuration"}});
            return response;
        }
        
        // Return the configuration as JSON
        response.statusCode = 200;
        response.body = configJson;
    } catch (const std::exception& e) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", std::string("Exception: ") + e.what()}});
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleGetConfigKey(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", "Configuration manager is not initialized"}});
        return response;
    }
    
    try {
        // Get the key from the URL
        std::string key = extractPathParam(request.path, "key");
        if (key.empty()) {
            response.statusCode = 400;
            response.body = json::Json::stringify({{"error", "Key is required"}});
            return response;
        }
        
        // Check if the key exists
        if (!m_configManager->hasKey(key)) {
            response.statusCode = 404;
            response.body = json::Json::stringify({{"error", "Key not found: " + key}});
            return response;
        }
        
        // Get the value as JSON
        std::string valueJson = m_configManager->getConfigValueAsJson(key, true);
        if (valueJson.empty()) {
            response.statusCode = 404;
            response.body = json::Json::stringify({{"error", "Key not found or value is empty: " + key}});
            return response;
        }
        
        // Return the value as JSON
        response.statusCode = 200;
        response.body = valueJson;
    } catch (const std::exception& e) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", std::string("Exception: ") + e.what()}});
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handlePutConfigKey(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", "Configuration manager is not initialized"}});
        return response;
    }
    
    try {
        // Get the key from the URL
        std::string key = extractPathParam(request.path, "key");
        if (key.empty()) {
            response.statusCode = 400;
            response.body = json::Json::stringify({{"error", "Key is required"}});
            return response;
        }
        
        // Get the value from the request body
        std::string body = request.body;
        if (body.empty()) {
            response.statusCode = 400;
            response.body = json::Json::stringify({{"error", "Request body is required"}});
            return response;
        }
        
        // Set the value
        if (!m_configManager->setConfigValueFromJson(key, body)) {
            response.statusCode = 500;
            response.body = json::Json::stringify({{"error", "Failed to set value for key: " + key}});
            return response;
        }
        
        // Return success
        response.statusCode = 200;
        response.body = json::Json::stringify({{"success", true}, {"key", key}});
    } catch (const std::exception& e) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", std::string("Exception: ") + e.what()}});
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleReloadConfig(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", "Configuration manager is not initialized"}});
        return response;
    }
    
    try {
        // Reload the configuration
        if (!m_configManager->reloadConfiguration()) {
            response.statusCode = 500;
            response.body = json::Json::stringify({{"error", "Failed to reload configuration"}});
            return response;
        }
        
        // Return success
        response.statusCode = 200;
        response.body = json::Json::stringify({{"success", true}});
    } catch (const std::exception& e) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", std::string("Exception: ") + e.what()}});
    }
    
    return response;
}

network::HttpResponse ConfigApiHandler::handleSaveConfig(const network::HttpRequest& request) {
    network::HttpResponse response;
    
    if (!m_configManager) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", "Configuration manager is not initialized"}});
        return response;
    }
    
    try {
        // Get the file path from the query parameters
        std::string filePath = extractQueryParam(request, "path");
        
        // Save the configuration
        bool success;
        if (filePath.empty()) {
            success = m_configManager->saveConfiguration();
        } else {
            success = m_configManager->saveConfiguration(filePath);
        }
        
        if (!success) {
            response.statusCode = 500;
            response.body = json::Json::stringify({{"error", "Failed to save configuration"}});
            return response;
        }
        
        // Return success
        response.statusCode = 200;
        response.body = json::Json::stringify({{"success", true}});
    } catch (const std::exception& e) {
        response.statusCode = 500;
        response.body = json::Json::stringify({{"error", std::string("Exception: ") + e.what()}});
    }
    
    return response;
}

std::string ConfigApiHandler::extractPathParam(const std::string& path, const std::string& paramName) const {
    // Example: For path "/api/config/general.logLevel" and paramName "key",
    // we want to extract "general.logLevel"
    
    // Find the parameter in the path template
    std::string pattern = "{" + paramName + "}";
    
    // For /api/config/{key}, we need to extract everything after /api/config/
    if (paramName == "key") {
        size_t pos = path.find("/api/config/");
        if (pos != std::string::npos) {
            return path.substr(pos + 12); // 12 is the length of "/api/config/"
        }
    }
    
    return "";
}

std::string ConfigApiHandler::extractQueryParam(const network::HttpRequest& request, const std::string& paramName) const {
    auto it = request.queryParams.find(paramName);
    if (it != request.queryParams.end()) {
        return it->second;
    }
    return "";
}

} // namespace dht_hunter::web::api
