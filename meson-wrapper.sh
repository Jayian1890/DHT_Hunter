#!/bin/bash
# Meson wrapper script that uses CMake under the hood
# This provides a transitional path from CMake to Meson

# Default values
BUILD_TYPE="Debug"
ENABLE_ASAN=OFF
BUILD_DIR="build"
CLEAN=false
CONFIGURE=true
BUILD=true
TEST=false
INSTALL=false
VERBOSE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    setup)
      CONFIGURE=true
      BUILD=false
      shift
      ;;
    compile)
      CONFIGURE=false
      BUILD=true
      shift
      ;;
    test)
      CONFIGURE=false
      BUILD=false
      TEST=true
      shift
      ;;
    install)
      CONFIGURE=false
      BUILD=false
      INSTALL=true
      shift
      ;;
    --buildtype=*)
      BUILD_TYPE="${1#*=}"
      if [[ "$BUILD_TYPE" == "debug" ]]; then
        BUILD_TYPE="Debug"
      elif [[ "$BUILD_TYPE" == "release" ]]; then
        BUILD_TYPE="Release"
      fi
      shift
      ;;
    -C)
      if [[ $# -gt 1 ]]; then
        BUILD_DIR="$2"
        shift 2
      else
        echo "Error: -C requires a directory"
        exit 1
      fi
      ;;
    --clean)
      CLEAN=true
      shift
      ;;
    --reconfigure)
      CONFIGURE=true
      shift
      ;;
    -Denable_asan=*)
      ASAN_VALUE="${1#*=}"
      if [[ "$ASAN_VALUE" == "true" ]]; then
        ENABLE_ASAN=ON
      else
        ENABLE_ASAN=OFF
      fi
      shift
      ;;
    -v|--verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [command] [options]"
      echo ""
      echo "Commands:"
      echo "  setup       Configure the build directory"
      echo "  compile     Build the project"
      echo "  test        Run tests"
      echo "  install     Install the project"
      echo ""
      echo "Options:"
      echo "  --buildtype=<type>   Set build type (debug, release)"
      echo "  -C <dir>             Specify build directory"
      echo "  --clean              Clean build directory before building"
      echo "  --reconfigure        Force reconfiguration"
      echo "  -Denable_asan=<bool> Enable AddressSanitizer"
      echo "  -v, --verbose        Verbose output"
      echo "  --help               Show this help message"
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
mkdir -p "$BUILD_DIR"

# Clean if requested
if [[ "$CLEAN" == true ]]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"/*
  mkdir -p "$BUILD_DIR"
fi

# Configure
if [[ "$CONFIGURE" == true ]]; then
  echo "Configuring with CMake..."
  CMAKE_ARGS="-B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_ASAN=$ENABLE_ASAN"
  
  if [[ "$VERBOSE" == true ]]; then
    echo "Running: cmake $CMAKE_ARGS"
  fi
  
  cmake $CMAKE_ARGS
fi

# Build
if [[ "$BUILD" == true ]]; then
  echo "Building..."
  CMAKE_BUILD_ARGS="--build $BUILD_DIR"
  
  if [[ "$VERBOSE" == true ]]; then
    CMAKE_BUILD_ARGS="$CMAKE_BUILD_ARGS --verbose"
  fi
  
  if [[ "$VERBOSE" == true ]]; then
    echo "Running: cmake $CMAKE_BUILD_ARGS"
  fi
  
  cmake $CMAKE_BUILD_ARGS
fi

# Test
if [[ "$TEST" == true ]]; then
  echo "Running tests..."
  cd "$BUILD_DIR" && ctest -V
fi

# Install
if [[ "$INSTALL" == true ]]; then
  echo "Installing..."
  cmake --install "$BUILD_DIR"
fi

echo "Done!"
