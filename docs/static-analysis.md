# Static Analysis for DHT-Hunter

This document explains how to use the static analysis tools configured for the DHT-Hunter project.

## Using clang-tidy

### Option 1: Enable during build (Integrated)

You can enable clang-tidy during the build process by setting the `ENABLE_CLANG_TIDY` option:

```bash
# Configure with clang-tidy enabled
mkdir -p build
cd build
cmake -DENABLE_CLANG_TIDY=ON ..

# Build with clang-tidy analysis
make
```

With this approach:
- Warnings will be shown during the build process
- The build will be slower due to the analysis
- You can disable it again by setting `ENABLE_CLANG_TIDY=OFF`

### Option 2: Run separately (Recommended)

You can also run clang-tidy separately using the provided script:

```bash
# First, generate compile_commands.json
mkdir -p build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
cd ..

# Run the analysis script
./scripts/run-clang-tidy.sh
```

With this approach:
- You can run the analysis only when needed
- It doesn't slow down your regular builds
- You get more detailed output

## CLion Integration

CLion has built-in support for clang-tidy. To use it:

1. Go to Settings/Preferences → Editor → Inspections → C/C++ → General → Clang-Tidy
2. Enable the inspection
3. CLion will use the `.clang-tidy` file in the project root

## Disabling Analysis

### For the entire project:
```bash
# Configure without clang-tidy
cmake -DENABLE_CLANG_TIDY=OFF ..
```

### For specific lines:
```cpp
// For a single line:
int x = 42; // NOLINT

// For a block of code:
// NOLINTBEGIN
int x = 42;
int y = 43;
// NOLINTEND
```

### For specific checks:
```cpp
int x = 42; // NOLINT(readability-magic-numbers)
```

## Customizing Checks

The checks are configured in the `.clang-tidy` file in the project root. You can edit this file to enable or disable specific checks.
