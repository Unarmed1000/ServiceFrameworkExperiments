# ServiceFramework2025

⚠️ **Experimental Playground** - This is an experimental project for exploring C++20 coroutines with Boost.Asio

A modern C++20 project demonstrating asynchronous service architecture using coroutines (`co_await`, `co_return`) and Boost.Asio's awaitable types. This playground explores:

- **C++20 Coroutines**: Native coroutine support with `boost::asio::awaitable`
- **Boost.Asio**: Asynchronous I/O and coroutine integration
- **Service Architecture**: Thread group-based service model with priority-driven initialization
- **CMake + Conan**: Modern C++ build and dependency management
- **Unit Testing**: Comprehensive test coverage with GoogleTest

This is a learning and experimentation environment, not production code.

## Project Structure

### Test1 - Basic Service Framework
A simple proof-of-concept demonstrating:
- Basic `ServiceBase` class with thread-per-service model
- Individual services (Add, Subtract, Multiply, Divide, Calculator)
- Coroutine-based async operations using `co_await`
- Simple service composition (Calculator uses other services)
- **Unit tests** for the service framework

**Note**: Test1's service framework is a minimal implementation where each service manually creates its own thread.

### Test2 - Advanced Service Framework (In Progress)
A production-grade service framework with proper architecture:

- **Lifecycle Management**: Orchestrates service startup and shutdown (in development)
  - `LifecycleManager`: Central orchestrator for the entire service lifecycle
  - Extracts registrations from `ServiceRegistry`
  - Creates and manages `ManagedThreadHost` instances per thread group
  - Priority-based startup: highest priority services start first across all threads, then proceeds to lower priorities
  - Reverse-order shutdown: lowest priority services stop first, then higher priorities
  - Thread teardown after all services are stopped

- **Host Management**: Thread group-based service hosting
  - `ServiceHostBase`: Abstract base with shared lifecycle logic
  - `CooperativeThreadServiceHost`: For UI/main thread with poll-based execution
  - `ManagedThreadServiceHost`: Owns dedicated thread with `io_context`
  - `ManagedThreadHost`: Manages thread lifecycle
  - `ManagedThreadServiceProvider`: Per-thread service provider with priority groups

- **Service Registry**: Central registration and discovery of services
  - Priority-based service launch ordering
  - Thread group assignment for services
  - Service registration records with factory patterns
  - One-time extraction semantics

- **Service Provider**: Dependency injection and service resolution
  - `IServiceProvider`: Interface for service lookup
  - `ServiceProvider`: Type-safe wrapper with template methods
  - `ServiceProviderProxy`: Proxy with disconnect capability for rollback
  - Thread-local service maps

- **Service Lifecycle**: Comprehensive async lifecycle management
  - `IService`: Base interface for all services
  - `IServiceControl`: Control interface with `InitAsync`, `Process`, `ShutdownAsync`
  - `IServiceFactory`: Factory pattern for service creation
  - `ASyncServiceBase`: Base implementation for async services
  - `ProcessResult`: Indicates sleep preferences and quit status

- **Common Utilities**:
  - `AggregateException`: Multi-exception aggregation and handling
  - `SpdLogHelper`: Logging utilities

- **Unit Tests**:
  - Service registry functionality
  - Aggregate exception handling
  - Managed thread service provider
  - Service host base
  - Process result

**Components Implemented:**
- Service interfaces and lifecycle enums (Init/Process/Shutdown results)
- Service registry with priority and thread group support
- Service provider with dependency injection
- Async service base class
- Host implementations (`ServiceHostBase`, `CooperativeThreadServiceHost`, `ManagedThreadServiceHost`)
- Thread management (`ManagedThreadHost`, `ManagedThreadServiceProvider`)
- Exception types for registry, provider, and host
- Unit test coverage for core components

**Components In Development:**
- `LifecycleManager`: Orchestrated multi-threaded service launching with priority coordination

### Test3 - Placeholder
Simple test program demonstrating Boost.System integration. Reserved for future experiments.

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

> **Note:** The default build uses the `windows-vs2026` preset. Build directory structure is `build/{preset-name}/build/`.

### 1. Install Dependencies with Conan

```powershell
conan install . --output-folder=build/windows-vs2026 --build=missing
```

### 2. Configure CMake

```powershell
cmake --preset conan-default
```

### 3. Build

For Debug:
```powershell
cmake --build build/windows-vs2026/build --config Debug
```

For Release:
```powershell
cmake --build build/windows-vs2026/build --config Release
```

Or use VS Code's CMake Tools extension to build with the UI.

### 4. Run

For Debug:
```powershell
.\build\windows-vs2026\build\Debug\main_app.exe
.\build\windows-vs2026\build\Debug\test1.exe
.\build\windows-vs2026\build\Debug\test_service_registry.exe
```

For Release:
```powershell
.\build\windows-vs2026\build\Release\main_app.exe
.\build\windows-vs2026\build\Release\test1.exe
```

## Quick Start (One-liner)

```powershell
conan install . --output-folder=build/windows-vs2026 --build=missing ; cmake --preset conan-default ; cmake --build build/windows-vs2026/build --config Debug
```

## Using Profiles

Profiles allow you to use different generators (Visual Studio, Ninja, etc.) with isolated build directories:

```powershell
# Visual Studio multi-config (supports Debug/Release in same build)
conan install . --profile=profiles/windows-vs-debug --output-folder=build/vs --build=missing

# Ninja single-config (requires separate build directories per configuration)
conan install . --profile=profiles/windows-ninja-debug --output-folder=build/ninja-debug --build=missing
conan install . --profile=profiles/windows-ninja-release --output-folder=build/ninja-release --build=missing
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
├── LICENSE              # 0BSD License
├── notes.md             # Development notes and plans
├── profiles/            # Conan profiles for different generators
├── scripts/             # Build automation and Python utilities
├── src/                 # Source files
│   ├── main.cpp
│   ├── test1.cpp
│   ├── test2.cpp
│   ├── test3.cpp
│   └── Test2/
│       └── Service/
│           └── Provider/
│               └── ServiceProvider.cpp
├── include/             # Header files
│   ├── Common/          # Shared utilities (AggregateException, SpdLogHelper, etc.)
│   ├── Test1/           # Test1 service framework headers
│   └── Test2/
│       ├── Framework/
│       │   ├── Exception/   # Framework-specific exceptions
│       │   ├── Host/        # Thread management and hosting
│       │   ├── Lifecycle/   # LifecycleManager orchestration
│       │   ├── Provider/    # Dependency injection
│       │   ├── Registry/    # Service registration system
│       │   └── Service/     # Service interfaces and lifecycle
│       └── Services/        # Concrete service implementations
├── UnitTest/            # Unit tests
│   ├── Common/          # Common utility tests
│   ├── Test1/           # Test1 framework tests
│   └── Test2/
│       ├── Registry/    # Registry tests
│       └── Host/        # Host tests
└── build/               # Build outputs (not in git)
    └── windows-vs2026/  # Default build directory
        └── build/       # CMake build output
            ├── Debug/   # Debug executables
            └── Release/ # Release executables
```

## Dependencies

- **Boost 1.84.0**: Including Boost.Asio for asynchronous I/O
- **GoogleTest**: Unit testing framework
- **spdlog**: Fast C++ logging library
- **fmt**: Modern formatting library

## Executables

The project builds multiple executables:

- **main_app**: Main application entry point
- **test1**: Test1 framework with unit tests (service framework basics)
- **test2**: Test2 framework demonstration (Boost.Asio timer example)
- **test3**: Test3 placeholder (Boost.System example)
- **test_service_registry**: Unit tests for service registry
- **test_aggregate_exception**: Unit tests for AggregateException
- **test_managed_thread_service_provider**: Unit tests for managed thread service provider

Run any executable from the build directory:
```powershell
# Debug builds
.\build\windows-vs2026\build\Debug\test1.exe
.\build\windows-vs2026\build\Debug\test_service_registry.exe

# Release builds
.\build\windows-vs2026\build\Release\test1.exe
```

## Architecture Highlights

### Service Registry Pattern
- Services register with priority levels and thread group assignments
- Factory pattern for service instantiation
- Type-safe service lookup using `std::type_index`

### Dependency Injection
- Interface-based service resolution
- Services receive `ServiceCreateInfo` with provider access
- Thread-local service maps for concurrent access

### Async Lifecycle
Services implement a three-phase lifecycle:
1. **Init**: `co_await InitAsync(info)` - Initialize with dependencies
2. **Process**: `Process()` - Custom processing
3. **Shutdown**: `co_await ShutdownAsync()` - Clean shutdown

### Exception Handling
- `AggregateException`: Collects and reports multiple exceptions
- Specific exception types for registry, provider, and host errors

## Architecture Documentation

For detailed architecture diagrams of the Test2 service framework, see [`docs/class-diagrams.md`](docs/class-diagrams.md). This includes:

- High-level layer overview
- Class diagrams for each framework layer (Lifecycle, Host, Registry, Provider, Service)
- Service implementation patterns
- Exception hierarchy
- Startup and shutdown sequence diagrams

## License

Zero-Clause BSD (0BSD) - See LICENSE file for details.
