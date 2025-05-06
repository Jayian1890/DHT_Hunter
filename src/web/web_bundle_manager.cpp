#include "dht_hunter/web/web_bundle_manager.hpp"
#include "dht_hunter/web/bundle/web_bundle.hpp"
#include <algorithm>

using namespace dht_hunter::web_bundle;

namespace dht_hunter::web {

WebBundleManager::WebBundleManager(std::shared_ptr<network::HttpServer> httpServer)
    : m_httpServer(httpServer) {
    // Load bundled files
    m_files = web_bundle::get_bundled_files();
    m_filePaths = web_bundle::get_bundled_file_paths();
    m_mimeTypes = web_bundle::get_bundled_file_mime_types();

    unified_event::logDebug("Web.BundleManager", "Loaded " + std::to_string(m_files.size()) + " bundled web files");

    // Log the loaded files for debugging
    unified_event::logDebug("Web.BundleManager", "Bundled files:");
    for (const auto& [path, _] : m_files) {
        unified_event::logDebug("Web.BundleManager", "  - File: " + path);
    }

    unified_event::logDebug("Web.BundleManager", "File paths:");
    for (const auto& [path, pathValue] : m_filePaths) {
        unified_event::logDebug("Web.BundleManager", "  - Path: " + path + " -> " + pathValue);
    }

    unified_event::logDebug("Web.BundleManager", "MIME types:");
    for (const auto& [path, mimeType] : m_mimeTypes) {
        unified_event::logDebug("Web.BundleManager", "  - MIME: " + path + " -> " + mimeType);
    }
}

void WebBundleManager::registerRoutes() {
    if (!m_httpServer) {
        unified_event::logError("Web.BundleManager", "HTTP server is null, cannot register routes");
        return;
    }

    // Register specific routes for each file type

    // Register index.html for root path
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling root path request");
            network::HttpRequest modifiedRequest = request;
            modifiedRequest.path = "/index.html";
            return handleBundledFileRequest(modifiedRequest);
        }
    );

    // Register index.html
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/index.html",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling index.html request");
            return handleBundledFileRequest(request);
        }
    );

    // Register CSS file
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/css/styles.css",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling CSS request");
            return handleBundledFileRequest(request);
        }
    );

    // Register JS files
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/js/dashboard.js",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling dashboard.js request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/js/peers.js",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling peers.js request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/js/infohash.js",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling infohash.js request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/js/infohashes.js",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling infohashes.js request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/js/messages.js",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling messages.js request");
            return handleBundledFileRequest(request);
        }
    );

    // Register specific routes for HTML files
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/peers.html",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling peers.html request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/infohash.html",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling infohash.html request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/infohashes.html",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling infohashes.html request");
            return handleBundledFileRequest(request);
        }
    );

    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/messages.html",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling messages.html request");
            return handleBundledFileRequest(request);
        }
    );

    // Register a catch-all route for any other bundled files
    m_httpServer->registerRoute(
        network::HttpMethod::GET,
        "/*",
        [this](const network::HttpRequest& request) {
            unified_event::logDebug("Web.BundleManager", "Handling catch-all request");
            return handleBundledFileRequest(request);
        }
    );

    unified_event::logDebug("Web.BundleManager", "Registered routes for bundled web files");
}

const std::vector<unsigned char>* WebBundleManager::getFile(const std::string& path) const {
    // Normalize the path
    std::string normalizedPath = path;
    if (normalizedPath.empty() || normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }

    // Remove the leading slash for lookup
    std::string lookupPath = normalizedPath.substr(1);

    unified_event::logDebug("Web.BundleManager", "Looking up file: " + lookupPath);

    // Direct lookup first
    auto it = m_files.find(lookupPath);
    if (it != m_files.end()) {
        unified_event::logDebug("Web.BundleManager", "Found file directly: " + lookupPath);
        return it->second;
    }

    // Try to find the file by path
    for (const auto& [filePath, filePathValue] : m_filePaths) {
        unified_event::logDebug("Web.BundleManager", "Checking: " + filePath + " == " + lookupPath + " or " + filePathValue + " == " + lookupPath);
        if (filePathValue == lookupPath || filePath == lookupPath) {
            auto fileIt = m_files.find(filePathValue);
            if (fileIt != m_files.end()) {
                unified_event::logDebug("Web.BundleManager", "Found file by path: " + lookupPath + " -> " + filePathValue);
                return fileIt->second;
            }
        }
    }

    unified_event::logWarning("Web.BundleManager", "File not found: " + lookupPath);
    return nullptr;
}

std::string WebBundleManager::getMimeType(const std::string& path) const {
    // Normalize the path
    std::string normalizedPath = path;
    if (normalizedPath.empty() || normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }

    // Remove the leading slash for lookup
    std::string lookupPath = normalizedPath.substr(1);

    // Direct lookup first
    auto it = m_mimeTypes.find(lookupPath);
    if (it != m_mimeTypes.end()) {
        return it->second;
    }

    // Find the MIME type in the bundled files
    for (const auto& [filePath, filePathValue] : m_filePaths) {
        if (filePathValue == lookupPath || filePath == lookupPath) {
            auto mimeIt = m_mimeTypes.find(filePathValue);
            if (mimeIt != m_mimeTypes.end()) {
                return mimeIt->second;
            }
        }
    }

    // Default MIME type
    return "application/octet-stream";
}

bool WebBundleManager::isFileBundled(const std::string& path) const {
    return getFile(path) != nullptr;
}

std::unordered_map<std::string, std::string> WebBundleManager::getAllFilePaths() const {
    return m_filePaths;
}

network::HttpResponse WebBundleManager::handleBundledFileRequest(const network::HttpRequest& request) {
    network::HttpResponse response;

    // Get the file path from the request
    std::string path = request.path;

    unified_event::logDebug("Web.BundleManager", "Handling request for bundled file: " + path);

    // Normalize the path
    std::string normalizedPath = path;
    if (normalizedPath.empty() || normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }
    // Remove the leading slash for lookup
    std::string lookupPath = normalizedPath.substr(1);

    // Special case for root path
    if (lookupPath.empty()) {
        lookupPath = "index.html";
        unified_event::logDebug("Web.BundleManager", "Root path requested, using index.html");
    }

    unified_event::logDebug("Web.BundleManager", "Looking up file: " + lookupPath);

    // Log all available files for debugging
    unified_event::logDebug("Web.BundleManager", "Available files:");
    for (const auto& [filePath, _] : m_files) {
        unified_event::logDebug("Web.BundleManager", "  - " + filePath);
    }

    // Handle CSS files
    if (lookupPath == "css/styles.css") {
        unified_event::logDebug("Web.BundleManager", "Serving CSS file");
        response.statusCode = 200;
        response.body = std::string(css_styles_css.begin(), css_styles_css.end());
        response.setContentType("text/css");
        return response;
    }

    // Handle JS files
    if (lookupPath == "js/dashboard.js") {
        unified_event::logDebug("Web.BundleManager", "Serving dashboard.js file");
        response.statusCode = 200;
        response.body = std::string(js_dashboard_js.begin(), js_dashboard_js.end());
        response.setContentType("application/javascript");
        return response;
    } else if (lookupPath == "js/peers.js") {
        unified_event::logDebug("Web.BundleManager", "Serving peers.js file");
        response.statusCode = 200;
        response.body = std::string(js_peers_js.begin(), js_peers_js.end());
        response.setContentType("application/javascript");
        return response;
    } else if (lookupPath == "js/infohash.js") {
        unified_event::logDebug("Web.BundleManager", "Serving infohash.js file");
        response.statusCode = 200;
        response.body = std::string(js_infohash_js.begin(), js_infohash_js.end());
        response.setContentType("application/javascript");
        return response;
    } else if (lookupPath == "js/infohashes.js") {
        unified_event::logDebug("Web.BundleManager", "Serving infohashes.js file");
        response.statusCode = 200;
        response.body = std::string(js_infohashes_js.begin(), js_infohashes_js.end());
        response.setContentType("application/javascript");
        return response;
    } else if (lookupPath == "js/messages.js") {
        unified_event::logDebug("Web.BundleManager", "Serving messages.js file");
        response.statusCode = 200;
        response.body = std::string(js_messages_js.begin(), js_messages_js.end());
        response.setContentType("application/javascript");
        return response;
    }

    // Handle HTML files
    if (lookupPath == "index.html") {
        unified_event::logDebug("Web.BundleManager", "Serving index.html file");
        response.statusCode = 200;
        response.body = std::string(index_html.begin(), index_html.end());
        response.setHtmlContentType();
        return response;
    } else if (lookupPath == "peers.html") {
        unified_event::logDebug("Web.BundleManager", "Serving peers.html file");
        response.statusCode = 200;
        response.body = std::string(peers_html.begin(), peers_html.end());
        response.setHtmlContentType();
        return response;
    } else if (lookupPath == "infohash.html") {
        unified_event::logDebug("Web.BundleManager", "Serving infohash.html file");
        response.statusCode = 200;
        response.body = std::string(infohash_html.begin(), infohash_html.end());
        response.setHtmlContentType();
        return response;
    } else if (lookupPath == "infohashes.html") {
        unified_event::logDebug("Web.BundleManager", "Serving infohashes.html file");
        response.statusCode = 200;
        response.body = std::string(infohashes_html.begin(), infohashes_html.end());
        response.setHtmlContentType();
        return response;
    } else if (lookupPath == "messages.html") {
        unified_event::logDebug("Web.BundleManager", "Serving messages.html file");
        response.statusCode = 200;
        response.body = std::string(messages_html.begin(), messages_html.end());
        response.setHtmlContentType();
        return response;
    }

    // Try the general lookup approach as a fallback
    auto it = m_files.find(lookupPath);
    if (it != m_files.end()) {
        unified_event::logDebug("Web.BundleManager", "Found file via general lookup: " + lookupPath);

        // Set the response
        response.statusCode = 200;
        response.body = std::string(it->second->begin(), it->second->end());

        // Determine MIME type based on file extension
        if (lookupPath.ends_with(".html")) {
            response.setHtmlContentType();
        } else if (lookupPath.ends_with(".css")) {
            response.setContentType("text/css");
        } else if (lookupPath.ends_with(".js")) {
            response.setContentType("application/javascript");
        } else {
            response.setContentType("application/octet-stream");
        }

        return response;
    }

    // File not found
    unified_event::logWarning("Web.BundleManager", "Bundled file not found: " + lookupPath);
    response.statusCode = 404;
    response.body = "File not found: " + lookupPath;
    response.setContentType("text/plain");
    return response;
}

} // namespace dht_hunter::web
