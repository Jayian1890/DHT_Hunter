// Auto-generated file - DO NOT EDIT
// Index of all bundled web files

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "peers_html.hpp"
#include "infohashes_html.hpp"
#include "index_html.hpp"
#include "infohash_html.hpp"
#include "messages_html.hpp"
#include "css_styles_css.hpp"
#include "js_infohash_js.hpp"
#include "js_peers_js.hpp"
#include "js_infohashes_js.hpp"
#include "js_dashboard_js.hpp"
#include "js_messages_js.hpp"

namespace bitscrape {
namespace web_bundle {

// Get all bundled files
inline std::unordered_map<std::string, const std::vector<unsigned char>*> get_bundled_files() {
    std::unordered_map<std::string, const std::vector<unsigned char>*> files;
    files["peers_html_path"] = &peers_html;
    files["infohashes_html_path"] = &infohashes_html;
    files["index_html_path"] = &index_html;
    files["infohash_html_path"] = &infohash_html;
    files["messages_html_path"] = &messages_html;
    files["css_styles_css_path"] = &css_styles_css;
    files["js_infohash_js_path"] = &js_infohash_js;
    files["js_peers_js_path"] = &js_peers_js;
    files["js_infohashes_js_path"] = &js_infohashes_js;
    files["js_dashboard_js_path"] = &js_dashboard_js;
    files["js_messages_js_path"] = &js_messages_js;
    return files;
}

// Get all bundled file paths
inline std::unordered_map<std::string, std::string> get_bundled_file_paths() {
    std::unordered_map<std::string, std::string> paths;
    paths["peers.html"] = peers_html_path;
    paths["infohashes.html"] = infohashes_html_path;
    paths["index.html"] = index_html_path;
    paths["infohash.html"] = infohash_html_path;
    paths["messages.html"] = messages_html_path;
    paths["css/styles.css"] = css_styles_css_path;
    paths["js/infohash.js"] = js_infohash_js_path;
    paths["js/peers.js"] = js_peers_js_path;
    paths["js/infohashes.js"] = js_infohashes_js_path;
    paths["js/dashboard.js"] = js_dashboard_js_path;
    paths["js/messages.js"] = js_messages_js_path;
    return paths;
}

// Get all bundled file MIME types
inline std::unordered_map<std::string, std::string> get_bundled_file_mime_types() {
    std::unordered_map<std::string, std::string> mime_types;
    mime_types[peers_html_path] = peers_html_mime;
    mime_types[infohashes_html_path] = infohashes_html_mime;
    mime_types[index_html_path] = index_html_mime;
    mime_types[infohash_html_path] = infohash_html_mime;
    mime_types[messages_html_path] = messages_html_mime;
    mime_types[css_styles_css_path] = css_styles_css_mime;
    mime_types[js_infohash_js_path] = js_infohash_js_mime;
    mime_types[js_peers_js_path] = js_peers_js_mime;
    mime_types[js_infohashes_js_path] = js_infohashes_js_mime;
    mime_types[js_dashboard_js_path] = js_dashboard_js_mime;
    mime_types[js_messages_js_path] = js_messages_js_mime;
    return mime_types;
}

// Get a bundled file by path
inline const std::vector<unsigned char>* get_bundled_file(const std::string& path) {
    auto files = get_bundled_files();
    auto it = files.find(path);
    if (it != files.end()) {
        return it->second;
    }
    return nullptr;
}

// Get a bundled file MIME type by path
inline std::string get_bundled_file_mime_type(const std::string& path) {
    auto mime_types = get_bundled_file_mime_types();
    auto it = mime_types.find(path);
    if (it != mime_types.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

}  // namespace web_bundle
}  // namespace {namespace}
