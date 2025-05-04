// Auto-generated file - DO NOT EDIT
// Index of all bundled web files

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "index_html.hpp"
#include "css_styles_css.hpp"
#include "js_dashboard_js.hpp"

namespace dht_hunter {
namespace web_bundle {

// Get all bundled files
inline std::unordered_map<std::string, const std::vector<unsigned char>*> get_bundled_files() {
    std::unordered_map<std::string, const std::vector<unsigned char>*> files;
    files["index_html_path"] = &index_html;
    files["css_styles_css_path"] = &css_styles_css;
    files["js_dashboard_js_path"] = &js_dashboard_js;
    return files;
}

// Get all bundled file paths
inline std::unordered_map<std::string, std::string> get_bundled_file_paths() {
    std::unordered_map<std::string, std::string> paths;
    paths["index.html"] = index_html_path;
    paths["css/styles.css"] = css_styles_css_path;
    paths["js/dashboard.js"] = js_dashboard_js_path;
    return paths;
}

// Get all bundled file MIME types
inline std::unordered_map<std::string, std::string> get_bundled_file_mime_types() {
    std::unordered_map<std::string, std::string> mime_types;
    mime_types[index_html_path] = index_html_mime;
    mime_types[css_styles_css_path] = css_styles_css_mime;
    mime_types[js_dashboard_js_path] = js_dashboard_js_mime;
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
