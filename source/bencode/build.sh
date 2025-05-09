#!/bin/bash

# Default build type
BUILD_TYPE="debug"
ENABLE_ASAN=false
BUILD_EXAMPLES=true

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --release)
      BUILD_TYPE="release"
      shift
      ;;
    --debug)
      BUILD_TYPE="debug"
      shift
      ;;
    --asan)
      ENABLE_ASAN=true
      shift
      ;;
    --no-examples)
      BUILD_EXAMPLES=false
      shift
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Create build directory if it doesn't exist
mkdir -p build

# Configure Meson
echo "Configuring Meson build..."
meson setup build \
  --buildtype=$BUILD_TYPE \
  -Denable_asan=$ENABLE_ASAN \
  -Dbuild_examples=$BUILD_EXAMPLES

# Build
echo "Building..."
meson compile -C build

echo "Build completed successfully!"
