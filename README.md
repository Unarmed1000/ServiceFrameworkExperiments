# ServiceFramework2025

⚠️ **Experimental Playground** - This is an experimental project for exploring C++20 coroutines with Boost.Asio

A modern C++20 project demonstrating asynchronous service architecture using coroutines (`co_await`, `co_return`) and Boost.Asio's awaitable types. This playground explores:

- **C++20 Coroutines**: Native coroutine support with `boost::asio::awaitable`
- **Boost.Asio**: Asynchronous I/O and coroutine integration
- **Service Architecture**: Thread-per-service model with coroutine-based async operations
- **CMake + Conan**: Modern C++ build and dependency management

This is a learning and experimentation environment, not production code.

## Project Structure

### Test1 - Basic Service Framework (Current)
A simple proof-of-concept demonstrating:
- Basic `ServiceBase` class with thread-per-service model
- Individual services (Add, Subtract, Multiply, Divide, Calculator)
- Coroutine-based async operations using `co_await`
- Simple service composition (Calculator uses other services)

**Note**: Test1's service framework is a minimal dummy implementation - each service manually creates its own thread.

### Test2 - Advanced Service Framework (Planned)
Expanding the framework with proper architecture:
- **Service Registry**: Central registration and discovery of services
- **Service Provider**: Dependency injection and service resolution
- **Interface-based Design**: Services access each other through registered interfaces
- **Thread Pool Management**: Intelligent thread allocation instead of thread-per-service
- **Service Lifecycle**: Proper initialization, shutdown, and dependency ordering

### Test3 - Future Explorations
Reserved for additional experiments and architectural patterns.

## Prerequisites

- CMake 3.23 or higher
- Conan 2.x
- C++20 compatible compiler (MSVC 2019+, GCC 10+, or Clang 10+)
- **Python 3.x** (optional, for automatic VS Code IntelliSense configuration)
  - Recommended: Install `psutil` package for enhanced VS Code detection (`pip install psutil`)

## VS Code IntelliSense

When using Visual Studio Code, IntelliSense configuration is automatically generated during CMake configuration. The system:

- **Detects VS Code environment** automatically (checks environment variables and process tree)
- **Generates** `.vscode/c_cpp_properties.json` with Conan package paths
- **Tracks changes** using hash-based detection to avoid redundant updates
- **Cleans up** orphaned configuration files when build types are removed
- **Silent operation** when not in VS Code or when no changes detected

### Manual IntelliSense Update

If Python is not available or you need to manually regenerate the configuration:

```powershell
python scripts/update_vscode_includes.py --workspace-root .
```

### Requirements

- CMake Tools extension (`ms-vscode.cmake-tools`) recommended
- Python 3.x in PATH (automatically detected by CMake)
- Optional: `psutil` package for improved VS Code detection

## Building the Project

> **Breaking Change:** Build directory structure has changed to `build/{directory-name}/build/`. Existing `build/` directories must be deleted and rebuilt.

### 1. Install Dependencies with Conan

```powershell
conan install . --output-folder=build/default --build=missing
```

### 2. Configure CMake

```powershell
cmake --preset conan-default
```

### 3. Build

For Debug:
```powershell
cmake --build build/default/build --config Debug
```

For Release:
```powershell
cmake --build build/default/build --config Release
```

### 4. Run

For Debug:
```powershell
.\build\default\build\Debug\main_app.exe
.\build\default\build\Debug\test1.exe
```

For Release:
```powershell
.\build\default\build\Release\main_app.exe
.\build\default\build\Release\test1.exe
```

## Quick Start (One-liner)

```powershell
conan install . --output-folder=build/default --build=missing ; cmake --preset conan-default ; cmake --build build/default/build --config Debug
```

## Using Profiles

Profiles allow you to use different generators (Visual Studio, Ninja, etc.) with isolated build directories:

```powershell
# Visual Studio multi-config (supports Debug/Release in same build)
conan install . --profile=profiles/vs-debug --output-folder=build/vs --build=missing

# Ninja single-config (requires separate build directories per configuration)
conan install . --profile=profiles/ninja-debug --output-folder=build/ninja-debug --build=missing
conan install . --profile=profiles/ninja-release --output-folder=build/ninja-release --build=missing
```

**Note:** When adding new build directories, add the corresponding include to `CMakeUserPresets.json`:
```json
"include": [
    "build/{your-directory}/build/generators/CMakePresets.json"
]
```

See [`profiles/README.md`](profiles/README.md) for more details.

## Project Structure

```
ServiceFramework2025/
├── CMakeLists.txt       # Main CMake configuration
├── CMakePresets.json    # CMake presets for Conan integration
├── CMakeUserPresets.json# User-managed preset includes
├── conanfile.txt        # Conan dependencies
├── README.md            # This file
├── profiles/            # Conan profiles for different generators
├── scripts/             # Build automation scripts
├── src/                 # Source files
├── include/             # Header files
└── build/               # Build outputs (not in git)
    └── default/         # Default build directory
        └── build/       # CMake build output
            ├── Debug/   # Debug executables
            └── Release/ # Release executables
```

## Dependencies

- **Boost 1.84.0**: Including Boost.Asio for asynchronous I/O

## Features

- Modern C++20 standards
- Conan 2.x for dependency management
- CMake presets for easy configuration
- Boost.Asio for network programming
- Cross-platform support
