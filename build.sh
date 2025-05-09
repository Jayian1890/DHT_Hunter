#!/bin/bash
# Build script for BitScrape using Meson

# Default build type
BUILD_TYPE="debug"
ENABLE_ASAN=false
BUILD_TESTS=true
BUILD_APP_BUNDLE=true

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
    --no-tests)
      BUILD_TESTS=false
      shift
      ;;
    --no-app-bundle)
      BUILD_APP_BUNDLE=false
      shift
      ;;
    --clean)
      echo "Cleaning build directory..."
      rm -rf build
      shift
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --release         Build in release mode"
      echo "  --debug           Build in debug mode (default)"
      echo "  --asan            Enable AddressSanitizer"
      echo "  --no-tests        Disable building tests"
      echo "  --no-app-bundle   Disable building macOS app bundle"
      echo "  --clean           Clean build directory before building"
      echo "  --help            Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
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
  -Dbuild_tests=$BUILD_TESTS \
  -Dbuild_app_bundle=$BUILD_APP_BUNDLE

# Build
echo "Building..."
meson compile -C build

# Run tests if enabled
if [ "$BUILD_TESTS" = true ]; then
  echo "Running tests..."
  meson test -C build
fi

echo "Build complete!"
