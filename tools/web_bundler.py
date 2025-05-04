#!/usr/bin/env python3
"""
Web Bundler - Converts web files to C++ header files

This script converts web files (HTML, CSS, JS, etc.) to C++ header files
that can be included in the program. This allows the web files to be
bundled with the executable without needing to be copied separately.
"""

import os
import sys
import argparse
import base64
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

def process_file(file_path, output_dir, namespace):
    """Process a single file and convert it to a C++ header."""
    with open(file_path, 'rb') as f:
        content = f.read()

    # Get the relative path from the web directory
    rel_path = os.path.relpath(file_path, 'web')

    # Create variable name from the file path
    var_name = sanitize_var_name(rel_path)

    # Create the header file content
    header_content = f"""// Auto-generated file - DO NOT EDIT
// Generated from {rel_path}

#pragma once

#include <string>
#include <vector>

namespace {namespace} {{
namespace web_bundle {{

// Content of {rel_path}
inline const std::vector<unsigned char> {var_name} = {{
"""

    # Add the file content as a byte array
    for i, byte in enumerate(content):
        if i % 12 == 0:
            header_content += "\n    "
        header_content += f"0x{byte:02x}, "

    # Close the array and namespace
    header_content += f"""
}};

// File path
inline const std::string {var_name}_path = "{rel_path}";

// MIME type
inline const std::string {var_name}_mime = "{get_mime_type(file_path)}";

}} // namespace web_bundle
}} // namespace {namespace}
"""

    # Create the output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Write the header file
    output_path = os.path.join(output_dir, f"{var_name}.hpp")
    with open(output_path, 'w') as f:
        f.write(header_content)

    return rel_path, var_name

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

def generate_index_file(files, output_dir, namespace):
    """Generate an index file that includes all the bundled files."""
    index_content = f"""// Auto-generated file - DO NOT EDIT
// Index of all bundled web files

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

"""

    # Include all the generated headers
    for _, var_name in files:
        index_content += f"#include \"{var_name}.hpp\"\n"

    index_content += f"""
namespace {namespace} {{
namespace web_bundle {{

// Get all bundled files
inline std::unordered_map<std::string, const std::vector<unsigned char>*> get_bundled_files() {{
    std::unordered_map<std::string, const std::vector<unsigned char>*> files;
"""

    # Add all files to the map
    for _, var_name in files:
        index_content += f"    files[\"{var_name}_path\"] = &{var_name};\n"

    index_content += """    return files;
}

// Get all bundled file paths
inline std::unordered_map<std::string, std::string> get_bundled_file_paths() {
    std::unordered_map<std::string, std::string> paths;
"""

    # Add all file paths to the map
    for rel_path, var_name in files:
        index_content += f"    paths[\"{rel_path}\"] = {var_name}_path;\n"

    index_content += """    return paths;
}

// Get all bundled file MIME types
inline std::unordered_map<std::string, std::string> get_bundled_file_mime_types() {
    std::unordered_map<std::string, std::string> mime_types;
"""

    # Add all MIME types to the map
    for _, var_name in files:
        index_content += f"    mime_types[{var_name}_path] = {var_name}_mime;\n"

    index_content += """    return mime_types;
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

}}  // namespace web_bundle
}}  // namespace {namespace}
"""

    # Write the index file
    output_path = os.path.join(output_dir, "web_bundle.hpp")
    with open(output_path, 'w') as f:
        f.write(index_content)

def main():
    parser = argparse.ArgumentParser(description='Convert web files to C++ headers')
    parser.add_argument('--web-dir', default='web', help='Directory containing web files')
    parser.add_argument('--output-dir', default='include/dht_hunter/web/bundle', help='Output directory for headers')
    parser.add_argument('--namespace', default='dht_hunter', help='C++ namespace to use')
    args = parser.parse_args()

    # Get all files in the web directory
    web_files = []
    for root, _, files in os.walk(args.web_dir):
        for file in files:
            if file.startswith('.'):  # Skip hidden files
                continue
            file_path = os.path.join(root, file)
            web_files.append(file_path)

    # Process each file
    processed_files = []
    for file_path in web_files:
        rel_path, var_name = process_file(file_path, args.output_dir, args.namespace)
        processed_files.append((rel_path, var_name))
        print(f"Processed {rel_path} -> {var_name}.hpp")

    # Generate the index file
    generate_index_file(processed_files, args.output_dir, args.namespace)
    print(f"Generated web_bundle.hpp with {len(processed_files)} files")

if __name__ == '__main__':
    main()
