# Service Framework Experiments

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

### Test2 - Advanced Service Framework
A production-grade service framework with comprehensive architecture demonstrating C++20 coroutines, Boost.Asio async patterns, and sophisticated service lifecycle management.

#### Framework Components

**Service Architecture** (`Framework/Service/`):
- `IService`: Base interface for all services
- `IServiceControl`: Lifecycle control with `InitAsync`, `Process`, `ShutdownAsync`
- `IServiceControlBase`: Base control interface with process management
- `IServiceProxyControl`: Control interface for proxy services
- `AsyncServiceBase`: Base class for async services with lifecycle defaults
- `AsyncServiceProxyBase`: Base class for cross-thread service proxies
- `ProcessResult`: Service processing state (sleep limits, quit status)
- `ServiceInitResult`, `ServiceShutdownResult`: Lifecycle operation results
- `ServiceCreateInfo`, `ServiceProxyCreateInfo`: Service instantiation context

**Service Factory Pattern** (`Framework/Service/Async/`):
- `IAsyncServiceFactory`: Combined factory interface for both impl and proxy factories
- `IAsyncServiceImplFactory`: Factory interface for service implementations
- `IAsyncServiceProxyFactory`: Factory interface for service proxies
- `AsyncServiceFactory`: Abstract factory base class combining impl and proxy factories
- `AsyncServiceImplFactory`: Base implementation factory
- `AsyncServiceProxyFactory`: Base proxy factory
- `AsyncServiceFactoryUtil`: Utilities for factory type checks
- **Thread-Safety Requirement**: All factory implementations MUST be immutable and thread-safe, allowing safe concurrent access across multiple threads without synchronization

**Lifecycle Management** (`Framework/Lifecycle/`):
- `LifecycleManager`: Orchestrates service startup/shutdown across thread groups
  - Priority-based startup: highest priority services start first (across all threads)
  - Reverse-order shutdown: lowest priority services stop first
  - Rollback support on initialization failure
  - Coordinates `CooperativeThreadHost` (main thread) and `ManagedThreadHost` instances
- `LifecycleManagerConfig`: Configuration for lifecycle behavior
- `ExecutorContext`: Thread-safe lifetime tracking with weak pointer semantics
- `DispatchContext`: Combines executor and dispatcher for cross-thread operations
- `ILifeTracker`: Interface for lifetime-trackable objects

**Thread Management & Hosting** (`Framework/Host/`):
- `IServiceHost`: Abstract host interface
- `CooperativeThreadHost` (`Host/Cooperative/`): Poll-based execution for main thread
- `ManagedThreadHost` (`Host/Managed/`): Dedicated thread with `io_context`
  - `ManagedThreadServiceHost`: Service hosting on managed thread
  - `ManagedThreadServiceProvider`: Per-thread service provider with priority groups
- `ServiceHostProxy`: Proxy pattern for host operations
- `ThreadRecord`: Thread group metadata and state
- `StartServiceRecord`, `StartedServiceInfo`: Service startup tracking
- `ServiceInstanceInfo`: Runtime service instance data

**Service Registry** (`Framework/Registry/`):
- `ServiceRegistry`: Central registration system for service factories
  - Priority-based launch ordering
  - Thread group assignment
  - Type-safe registration using `std::type_index`
  - One-time extraction semantics (ownership transfer to `LifecycleManager`)
- `IServiceRegistry`: Registry interface
- `ServiceRegistrationRecord`: Factory metadata (priority, thread group, factory)
- `ServiceThreadGroupId`: Type-safe thread group identifier
- `ServiceLaunchPriority`: Type-safe priority identifier

**Dependency Injection** (`Framework/Provider/`):
- `IServiceProvider`: Service lookup interface
  - `GetService()`: Throws on missing service
  - `TryGetService()`: Returns nullptr on missing service
  - `TryGetServices()`: Returns all services of a type
- `ServiceProvider`: Type-safe wrapper with template methods
  - `GetService<T>()`: Typed service retrieval with automatic casting
  - `TryGetService<T>()`, `GetServices<T>()`: Template convenience methods
- `ServiceProviderProxy`: Proxy with disconnect capability for rollback

**Cross-Thread Communication** (`Framework/Util/`):
- `AsyncProxyHelper`: Utilities for safe cross-thread async method invocation
  - Support for both executor and dispatch context patterns
  - Automatic lifetime checking using `ExecutorContext` or `DispatchContext`
  - Exception handling for disposed objects (`ServiceDisposedException`)

**Exception Handling** (`Framework/Exception/`):
- `ServiceProviderException`: Base for provider errors
- `UnknownServiceException`: Service not found
- `ServiceCastException`: Invalid service type cast
- `MultipleServicesFoundException`: Ambiguous service query
- `WrongThreadException`: Service accessed from wrong thread
- `ServiceDisposedException`: Service no longer alive
- `DuplicateServiceRegistrationException`: Duplicate factory registration
- `RegistryExtractedException`: Registry already extracted
- `InvalidServiceFactoryException`: Invalid factory configuration
- `InvalidPriorityOrderException`: Invalid priority ordering
- `EmptyPriorityGroupException`: Priority group has no services

#### Example Services (`Services/`)

The framework includes example service implementations demonstrating patterns:

- **AddService** (`Services/Add/`): Simple addition service
  - `IAddService`: Interface with `AddAsync(double, double)`
  - `AddServiceImplFactory`: Implementation factory
  - `AddServiceProxyFactory`: Cross-thread proxy factory

- **CalculatorService** (`Services/Calculator/`): Expression evaluation service
  - `ICalculatorService`: Interface with `EvaluateAsync(string expression)`
  - Demonstrates service composition (uses Add, Subtract, Multiply, Divide services)
  - `CalculatorServiceImplFactory`, `CalculatorServiceProxyFactory`: Factories

- **SubtractService** (`Services/Subtract/`): Subtraction service
- **MultiplyService** (`Services/Multiply/`): Multiplication service
- **DivideService** (`Services/Divide/`): Division service

All services follow the pattern:
1. Interface inherits from `IService`
2. Implementation inherits from `AsyncServiceBase` and implements interface
3. Proxy inherits from `AsyncServiceProxyBase` and implements interface (for cross-thread access)
4. Factory classes create instances based on `ServiceCreateInfo`/`ServiceProxyCreateInfo`

**Service Configuration** (`Services/ServiceConfig.hpp`):
- Centralized timing constants for service delays (for testing/demonstration)

#### Unit Tests (`UnitTest/Test2/`)

Comprehensive test coverage organized by framework layer:

- **Registry Tests** (`Registry/`):
  - `ServiceRegistryTest`: Registration, extraction, duplicate detection

- **Provider Tests** (`Provider/`):
  - `ServiceProviderTest`: Service lookup and retrieval
  - `ServiceProviderProxyTest`: Proxy disconnection and rollback

- **Host Tests** (`Host/`):
  - `ServiceHostBaseTest`: Base host lifecycle logic
  - `ManagedThreadServiceHostTest`: Managed thread behavior
  - `CooperativeThreadServiceHostTest`: Cooperative thread behavior

- **Lifecycle Tests** (`Lifecycle/`):
  - `LifecycleManagerTest`: Startup, shutdown, rollback orchestration
  - `ExecutorContextTest`: Lifetime tracking and weak pointer semantics
  - `DispatchContextTest`: Dispatcher and executor integration

- **Service Tests** (`Service/`):
  - `ProcessResultTest`: Process result enumeration behavior

- **Utility Tests** (`Util/`):
  - `AsyncProxyHelperTest`: Cross-thread async invocation patterns

**Key Architecture Features:**
- Priority-based service initialization across multiple thread groups
- Thread-safe service access with compile-time type safety
- Factory pattern for flexible service instantiation
- Proxy pattern for safe cross-thread communication
- Automatic rollback on initialization failures
- Weak pointer lifetime tracking to prevent dangling references
- Comprehensive exception handling with specific exception types

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
.\build\windows-vs2026\build\Debug\test_lifecycle_manager.exe
.\build\windows-vs2026\build\Debug\test_service_registry.exe
```

For Release:
```powershell
.\build\windows-vs2026\build\Release\main_app.exe
.\build\windows-vs2026\build\Release\test1.exe
.\build\windows-vs2026\build\Release\test_lifecycle_manager.exe
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
│   ├── Common/          # Common utility implementations
│   └── Test2/
│       └── Framework/   # Test2 framework implementations
│           ├── Host/        # Host implementations
│           ├── Lifecycle/   # Lifecycle manager implementation
│           ├── Provider/    # Service provider implementations
│           └── Registry/    # Service registry implementation
├── include/             # Header files
│   ├── Common/          # Shared utilities (AggregateException, SpdLogHelper, etc.)
│   ├── Test1/           # Test1 service framework headers
│   └── Test2/
│       ├── Framework/   # Framework core components
│       │   ├── Config/      # Configuration (empty/reserved)
│       │   ├── Exception/   # Framework-specific exceptions
│       │   ├── Host/        # Thread management and hosting
│       │   │   ├── Cooperative/    # Cooperative thread host
│       │   │   └── Managed/        # Managed thread host
│       │   ├── Lifecycle/   # Lifecycle orchestration and context tracking
│       │   ├── Provider/    # Dependency injection providers
│       │   ├── Registry/    # Service registration system
│       │   ├── Service/     # Service interfaces and lifecycle
│       │   │   └── Async/          # Async service patterns
│       │   └── Util/        # Cross-thread utilities (AsyncProxyHelper)
│       └── Services/    # Example service implementations
│           ├── Add/         # Addition service
│           ├── Calculator/  # Expression calculator service
│           ├── Divide/      # Division service
│           ├── Multiply/    # Multiplication service
│           └── Subtract/    # Subtraction service
├── UnitTest/            # Unit tests
│   ├── Common/          # Common utility tests
│   │   └── AggregateExceptionTest.cpp
│   ├── Test1/           # Test1 framework tests
│   │   └── ServiceFrameworkTest.cpp
│   └── Test2/
│       ├── Host/        # Host tests (4 tests)
│       ├── Lifecycle/   # Lifecycle and context tests (3 tests)
│       ├── Provider/    # Provider tests (2 tests)
│       ├── Registry/    # Registry tests (1 test)
│       ├── Service/     # Service tests (6 tests)
│       └── Util/        # Utility tests (2 tests)
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
- **test1**: Test1 service framework demonstration
- **test2**: Test2 Boost.Asio timer example
- **test3**: Test3 placeholder (Boost.System example)

**Test1 Unit Tests:**
- **test_add_service**: AddService functionality

**Test2 Unit Tests:**

*Registry Tests:*
- **test_service_registry**: Service registry registration, extraction, and validation

*Provider Tests:*
- **test_service_provider**: Service provider lookup and retrieval
- **test_service_provider_proxy**: Service provider proxy disconnection

*Host Tests:*
- **test_service_host_base**: Service host base class lifecycle
- **test_managed_thread_service_host**: Managed thread host behavior
- **test_cooperative_thread_service_host**: Cooperative thread host behavior
- **test_managed_thread_service_provider**: Per-thread service provider with priority groups
- **test_managed_thread_service_provider_staging**: Staging area functionality

*Lifecycle Tests:*
- **test_lifecycle_manager**: Complete lifecycle orchestration with startup/shutdown/rollback
- **test_executor_context**: Executor context lifetime tracking
- **test_dispatch_context**: Dispatch context functionality

*Service Tests:*
- **test_process_result**: Process result enumeration
- **test_add_service**: AddService implementation (Test2 version)
- **test_subtract_service**: SubtractService implementation
- **test_multiply_service**: MultiplyService implementation
- **test_divide_service**: DivideService implementation
- **test_calculator_service**: CalculatorService expression evaluation

*Utility Tests:*
- **test_async_proxy_helper**: Cross-thread async proxy utilities
- **test_mock_service_factory**: Mock factory for testing

**Common Utility Tests:**
- **test_aggregate_exception**: AggregateException multi-exception handling

Run any executable from the build directory:
```powershell
# Debug builds
.\build\windows-vs2026\build\Debug\test1.exe
.\build\windows-vs2026\build\Debug\test_lifecycle_manager.exe
.\build\windows-vs2026\build\Debug\test_calculator_service.exe

# Release builds
.\build\windows-vs2026\build\Release\test1.exe
.\build\windows-vs2026\build\Release\test_lifecycle_manager.exe
```

## Architecture Highlights (Test2)

### Lifecycle Management
The `LifecycleManager` orchestrates service startup and shutdown across multiple thread groups:
- **Priority-based coordination**: Services start in descending priority order (highest first) across all thread groups simultaneously
- **Reverse-order shutdown**: Services shut down in ascending priority order (lowest first) for clean dependency teardown
- **Rollback support**: If initialization fails, successfully started services are cleanly rolled back in reverse order
- **Thread coordination**: Manages a `CooperativeThreadHost` for the main thread (ID 0) and spawns `ManagedThreadHost` instances for worker threads
- **Lifetime tracking**: `ExecutorContext` and `DispatchContext` provide thread-safe weak pointer semantics to prevent dangling references

### Service Registry Pattern
The `ServiceRegistry` provides centralized service factory management:
- Services register via factory objects (`AsyncServiceImplFactory`, `AsyncServiceProxyFactory`)
- Each service has a priority level (`ServiceLaunchPriority`) and thread group assignment (`ServiceThreadGroupId`)
- Type-safe service identification using `std::type_index`
- One-time extraction: Ownership transfers to `LifecycleManager` via `ExtractRegistrations()`
- Prevents duplicate registrations and enforces validation rules

### Dependency Injection
Type-safe service resolution through the `IServiceProvider` interface:
- `GetService<T>()`: Retrieves strongly-typed service reference, throws if not found
- `TryGetService<T>()`: Returns `nullptr` if service doesn't exist
- `GetServices<T>()`: Returns all services implementing interface `T`
- Services receive `ServiceCreateInfo` with provider access during `InitAsync()`
- Thread-local service maps ensure concurrent access safety
- `ServiceProviderProxy` enables rollback by disconnecting service access

### Async Lifecycle (Three-Phase Pattern)
All services implement a consistent async lifecycle:
1. **Init Phase**: `co_await InitAsync(info)` - Async initialization with dependency injection
2. **Process Phase**: `Process()` - Synchronous per-frame/tick processing (returns `ProcessResult`)
3. **Shutdown Phase**: `co_await ShutdownAsync()` - Async cleanup and resource release

Base classes (`AsyncServiceBase`, `AsyncServiceProxyBase`) provide default implementations.

### Cross-Thread Communication
Safe async invocation across thread boundaries using `AsyncProxyHelper`:
- **Proxy pattern**: Services on thread A can safely call services on thread B
- **Lifetime checking**: Automatically validates target service still exists before invocation
- **Context support**: Works with both `ExecutorContext` (weak pointer) and `DispatchContext` (executor + dispatcher)
- **Exception handling**: Throws `ServiceDisposedException` if target service has been destroyed

### Factory Pattern & Proxy Services
Services use a dual-factory pattern for flexible deployment:
- **Implementation Factory** (`IAsyncServiceImplFactory`): Creates the actual service implementation
- **Proxy Factory** (`IAsyncServiceProxyFactory`): Creates a cross-thread proxy that forwards calls to the implementation
- **Combined Factory** (`IAsyncServiceFactory`): Unified interface combining both impl and proxy factory capabilities
- Each factory declares supported interfaces via `GetSupportedInterfaces()`
- Factories register with `ServiceRegistry` during application initialization
- **Immutability & Thread-Safety**: Factory implementations are immutable (state fixed after construction) and thread-safe (methods safely callable from multiple threads concurrently), allowing the framework to share factory instances across threads without synchronization overhead

### Thread Group Architecture
Services organize into thread groups for isolated execution contexts:
- **Main thread group** (ID 0): Cooperative, poll-based execution (`CooperativeThreadHost`)
- **Worker thread groups** (ID 1+): Dedicated threads with `boost::asio::io_context` (`ManagedThreadHost`)
- Each thread group has its own `ManagedThreadServiceProvider` for thread-local service access
- Priority levels ensure correct initialization order within and across thread groups

### Exception Handling
Comprehensive exception hierarchy for precise error reporting:
- **Registry exceptions**: `DuplicateServiceRegistrationException`, `RegistryExtractedException`, `InvalidServiceFactoryException`
- **Provider exceptions**: `UnknownServiceException`, `ServiceCastException`, `MultipleServicesFoundException`
- **Host exceptions**: `WrongThreadException`, `EmptyPriorityGroupException`, `InvalidPriorityOrderException`
- **Lifecycle exceptions**: `ServiceDisposedException` (cross-thread access to destroyed service)
- **Aggregation**: `AggregateException` collects multiple initialization/shutdown failures for batch reporting

## Architecture Documentation

For detailed architecture diagrams of the Test2 service framework, see [`docs/class-diagrams.md`](docs/class-diagrams.md). This includes:

- High-level layer overview
- Class diagrams for each framework layer (Lifecycle, Host, Registry, Provider, Service)
- Service implementation patterns
- Exception hierarchy
- Startup and shutdown sequence diagrams

## License

Zero-Clause BSD (0BSD) - See LICENSE file for details.
