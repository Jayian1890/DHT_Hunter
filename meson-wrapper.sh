#!/bin/bash
# Meson wrapper script
# This provides a simple wrapper for common Meson commands

# Default values
BUILD_TYPE="debug"
ENABLE_ASAN=false
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
        ENABLE_ASAN=true
      else
        ENABLE_ASAN=false
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
  echo "Configuring with Meson..."
  MESON_ARGS="setup $BUILD_DIR --buildtype=$BUILD_TYPE -Denable_asan=$ENABLE_ASAN"

  if [[ "$VERBOSE" == true ]]; then
    echo "Running: meson $MESON_ARGS"
  fi

  meson $MESON_ARGS
fi

# Build
if [[ "$BUILD" == true ]]; then
  echo "Building..."
  MESON_BUILD_ARGS="compile -C $BUILD_DIR"

  if [[ "$VERBOSE" == true ]]; then
    MESON_BUILD_ARGS="$MESON_BUILD_ARGS --verbose"
  fi

  if [[ "$VERBOSE" == true ]]; then
    echo "Running: meson $MESON_BUILD_ARGS"
  fi

  meson $MESON_BUILD_ARGS
fi

# Test
if [[ "$TEST" == true ]]; then
  echo "Running tests..."
  meson test -C "$BUILD_DIR" -v
fi

# Install
if [[ "$INSTALL" == true ]]; then
  echo "Installing..."
  meson install -C "$BUILD_DIR"
fi

echo "Done!"
