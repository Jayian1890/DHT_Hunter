#!/usr/bin/env python3
"""
Script to remove logger initialization and logging calls from C++ files.
This script handles complex patterns and preserves code structure.
"""

import os
import re
import sys
import glob
from pathlib import Path

# Regular expressions for matching logger patterns
# Logger member variable patterns
LOGGER_MEMBER_PATTERN = re.compile(r'\s*event::Logger\s+m_logger;')
LOGGER_MEMBER_PATTERN2 = re.compile(r'\s*Logger\s+m_logger;')

# Logger initialization patterns
LOGGER_INIT_PATTERN = re.compile(r',\s*m_logger\(event::Logger::forComponent\([^)]+\)\)')
LOGGER_INIT_PATTERN2 = re.compile(r'm_logger\(event::Logger::forComponent\([^)]+\)\)')
LOGGER_INIT_PATTERN3 = re.compile(r'\s*m_logger\s*=\s*event::Logger::forComponent\([^)]+\);')
LOGGER_INIT_PATTERN4 = re.compile(r'\s*m_logger\s*=\s*Logger::forComponent\([^)]+\);')
AUTO_LOGGER_PATTERN = re.compile(r'\s*auto\s+logger\s*=\s*dht_hunter::event::Logger::forComponent\([^)]+\);')
AUTO_LOGGER_PATTERN2 = re.compile(r'\s*auto\s+logger\s*=\s*event::Logger::forComponent\([^)]+\);')

# Logger call patterns
LOGGER_CALL_PATTERN = re.compile(r'\s*m_logger\.(info|debug|error|warning|trace|critical)\([^;]+\);')
LOGGER_CALL_PATTERN2 = re.compile(r'\s*logger\.(info|debug|error|warning|trace|critical)\([^;]+\);')

def process_file(file_path):
    """Process a single file to remove logger code."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Keep track of whether we made changes
        original_content = content

        # Remove logger member variables
        content = LOGGER_MEMBER_PATTERN.sub('    // Logger removed', content)
        content = LOGGER_MEMBER_PATTERN2.sub('    // Logger removed', content)

        # Remove logger initialization in constructor initializer list
        content = LOGGER_INIT_PATTERN.sub('', content)

        # Remove standalone logger initialization
        content = LOGGER_INIT_PATTERN2.sub('/* Logger initialization removed */', content)

        # Remove assignment-style logger initialization
        content = LOGGER_INIT_PATTERN3.sub('    // Logger initialization removed', content)
        content = LOGGER_INIT_PATTERN4.sub('    // Logger initialization removed', content)

        # Remove auto logger initialization
        content = AUTO_LOGGER_PATTERN.sub('    // Logger initialization removed', content)
        content = AUTO_LOGGER_PATTERN2.sub('    // Logger initialization removed', content)

        # Remove logger calls (info, debug, error, warning, etc.)
        content = LOGGER_CALL_PATTERN.sub('', content)
        content = LOGGER_CALL_PATTERN2.sub('', content)

        # Only write back if changes were made
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False

def main():
    """Main function to process all C++ files in the project."""
    # Get the project root directory
    project_root = os.getcwd()

    # Find all C++ source and header files
    cpp_files = glob.glob(f"{project_root}/**/*.cpp", recursive=True)
    hpp_files = glob.glob(f"{project_root}/**/*.hpp", recursive=True)
    all_files = cpp_files + hpp_files

    print(f"Found {len(all_files)} C++ files to process")

    # Process each file
    modified_count = 0
    for file_path in all_files:
        if process_file(file_path):
            modified_count += 1
            print(f"Modified: {os.path.relpath(file_path, project_root)}")

    print(f"\nSummary: Modified {modified_count} out of {len(all_files)} files")

if __name__ == "__main__":
    main()
