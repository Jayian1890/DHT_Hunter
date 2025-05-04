#!/usr/bin/env python3
"""
Bundle web files into C++ headers
"""

import os
import sys
import re
from pathlib import Path

def sanitize_var_name(name):
    """Sanitize a filename to be used as a C++ variable name."""
    # Replace non-alphanumeric characters with underscores
    name = re.sub(r'[^a-zA-Z0-9]', '_', name)
    # Ensure it doesn't start with a number
    if name[0].isdigit():
        name = '_' + name
    return name

def get_mime_type(file_path):
    """Get the MIME type for a file based on its extension."""
    ext = os.path.splitext(file_path)[1].lower()
    mime_types = {
        '.html': 'text/html',
        '.css': 'text/css',
        '.js': 'application/javascript',
        '.json': 'application/json',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg',
        '.gif': 'image/gif',
        '.svg': 'image/svg+xml',
        '.ico': 'image/x-icon',
    }
    return mime_types.get(ext, 'application/octet-stream')

def main():
    # Create the output directory
    output_dir = "include/dht_hunter/web/bundle"
    os.makedirs(output_dir, exist_ok=True)

    # Get all web files
    web_files = []
    for root, _, files in os.walk("web"):
        for file in files:
            if file.startswith('.'):  # Skip hidden files
                continue
            file_path = os.path.join(root, file)
            rel_path = os.path.relpath(file_path, "web")
            web_files.append((file_path, rel_path))

    # Generate the web_bundle.hpp file
    with open(os.path.join(output_dir, "web_bundle.hpp"), "w") as f:
        f.write("""// Auto-generated file - DO NOT EDIT
// Index of all bundled web files

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace dht_hunter {
namespace web_bundle {

""")

        # Add each file
        for file_path, rel_path in web_files:
            var_name = sanitize_var_name(rel_path)
            mime_type = get_mime_type(file_path)

            f.write(f"// Bundled file: {rel_path}\n")
            f.write(f"inline const std::vector<unsigned char> {var_name} = {{\n")

            # Read the file content
            with open(file_path, "rb") as file:
                content = file.read()

                # Write the file content as a byte array
                for i, byte in enumerate(content):
                    if i % 12 == 0:
                        f.write("\n    ")
                    f.write(f"0x{byte:02x}, ")

            f.write("\n};\n")
            f.write(f"inline const std::string {var_name}_path = \"{rel_path}\";\n")
            f.write(f"inline const std::string {var_name}_mime = \"{mime_type}\";\n\n")

        # Add the helper functions
        f.write("""// Get all bundled files
inline std::unordered_map<std::string, const std::vector<unsigned char>*> get_bundled_files() {
    std::unordered_map<std::string, const std::vector<unsigned char>*> files;
""")

        for _, rel_path in web_files:
            var_name = sanitize_var_name(rel_path)
            f.write(f"    files[\"{rel_path}\"] = &{var_name};\n")

        f.write("""    return files;
}

// Get all bundled file paths
inline std::unordered_map<std::string, std::string> get_bundled_file_paths() {
    std::unordered_map<std::string, std::string> paths;
""")

        for _, rel_path in web_files:
            var_name = sanitize_var_name(rel_path)
            f.write(f"    paths[\"{rel_path}\"] = {var_name}_path;\n")

        f.write("""    return paths;
}

// Get all bundled file MIME types
inline std::unordered_map<std::string, std::string> get_bundled_file_mime_types() {
    std::unordered_map<std::string, std::string> mime_types;
""")

        for _, rel_path in web_files:
            var_name = sanitize_var_name(rel_path)
            f.write(f"    mime_types[\"{rel_path}\"] = {var_name}_mime;\n")

        f.write("""    return mime_types;
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

} // namespace web_bundle
} // namespace dht_hunter
""")

    print(f"Generated web_bundle.hpp with {len(web_files)} files")

if __name__ == "__main__":
    main()
