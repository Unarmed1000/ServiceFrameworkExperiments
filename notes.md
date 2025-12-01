# Service Manager Implementation Plan

## Overview

A service manager that launches services across multiple thread groups using Boost.Asio io_contexts, with priority-based initialization orchestrated centrally. Each thread group maintains its own service provider map, receiving interface→instance mappings from higher-priority services for dependency resolution.

## Steps

### 1. Create ServiceManagerException classes

**File:** `include/Test2/Framework/Manager/ServiceManagerException.hpp`

Define exception types for:
- Empty registration lists
- Service initialization failures
- Lifecycle errors

### 2. Create ServiceManager class

**File:** `include/Test2/Framework/Manager/ServiceManager.hpp`

- Accepts `std::vector<ServiceRegistrationRecord>`
- Implements `IServiceProvider` (delegates to aggregate view of all thread providers)
- Creates one `boost::asio::io_context` per unique thread group (main=0 uses calling thread)
- Launches worker threads running `io_context::run()`

### 3. Implement per-thread-group providers

Create helper struct `ThreadGroupContext` containing:
- `boost::asio::io_context`
- Worker thread
- Thread-local service map: `std::unordered_map<std::type_index, std::shared_ptr<IServiceControl>>`

Store map of `ServiceThreadGroupId` to `ThreadGroupContext`.

### 4. Implement orchestrated priority-based launching

Manager orchestration sequence:
1. Sort registrations by priority descending
2. Group by priority level
3. For each priority level:
   - Collect current interface→instance mappings from all thread groups
   - Post async function to each relevant thread's io_context with:
     - List of factories for this thread+priority
     - Current interface→instance mappings
   - Async function on thread:
     - Updates its thread-local provider map with received mappings
     - Creates service instances from factories
     - Calls `co_await InitAsync()` with `ServiceCreateInfo` using thread-local provider
     - Adds new instances to thread-local map
     - Returns new interface→instance mappings to manager via `std::promise`
   - Manager waits for all futures at current priority level before proceeding to next

### 5. Implement aggregate IServiceProvider

- `GetService()`/`TryGetService()`: searches across all thread group provider maps with shared locking
- `TryGetServices()`: collects from all maps
- Services accessed cross-thread must handle thread-safety internally

### 6. Add shutdown orchestration

Shutdown sequence:
1. Reverse priority order
2. For each priority level:
   - Post async shutdown function to each thread group
   - Function calls `co_await ShutdownAsync()` on its services at that priority
   - Removes from thread-local map
   - Manager waits for all completions before next level
3. Stop io_contexts and join threads

## Further Considerations

### 1. Main thread execution
Provide `RunMainThreadGroup()` blocking method and `StopMainThreadGroup()` for clean exit from main loop?

### 2. Service mapping format
Pass `std::unordered_map<std::type_index, std::shared_ptr<IService>>` between manager and threads, or custom structure?

### 3. Init failure handling
If service fails `InitAsync()`, abort all launches or continue collecting errors? Should failure prevent lower-priority launches from accessing failed service's interfaces?

### 4. Thread-local provider implementation
Each thread needs `IServiceProvider` implementation wrapping its local map. Should this be a separate class or inline lambda/function object?

## Architecture

```
ServiceManager
├── ThreadGroupContext (per thread group)
│   ├── boost::asio::io_context
│   ├── std::thread (worker, except main=0)
│   └── std::unordered_map<std::type_index, std::shared_ptr<IServiceControl>>
│
├── Priority Level 1000 (example)
│   ├── Thread Group 0: Create & InitAsync services
│   ├── Thread Group 1: Create & InitAsync services
│   └── Wait for all to complete → collect new mappings
│
├── Priority Level 500 (example)
│   ├── Distribute mappings from level 1000 to all threads
│   ├── Thread Group 0: Create & InitAsync services
│   ├── Thread Group 2: Create & InitAsync services
│   └── Wait for all to complete → collect new mappings
│
└── ... continue for remaining priority levels
```

## Key Design Points

- **Thread affinity**: Each service instance is created, initialized, and destroyed only on its designated thread group thread
- **Dependency access**: Services can access higher-priority services via thread-local provider containing accumulated mappings
- **Orchestration**: Manager coordinates launch sequence from outside, posting work and waiting for completion
- **Main thread**: Thread group ID 0 runs on calling thread, all others get dedicated worker threads
- **Priority ordering**: Higher priority values launch first, can be dependencies for lower priority
