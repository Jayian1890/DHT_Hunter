#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Get the path to the actual executable
EXECUTABLE="$SCRIPT_DIR/BitScrape"

# Launch a new Terminal window and run the executable
osascript -e "tell application \"Terminal\"
    do script \"\\\"$EXECUTABLE\\\"\"
    activate
end tell"

# Exit this script immediately to prevent it from blocking
exit 0
