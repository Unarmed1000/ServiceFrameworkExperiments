# Test2 Service Framework Architecture

This document provides Mermaid class diagrams documenting the Test2 service framework architecture at various levels of abstraction.

## Table of Contents

- [High-Level Overview](#high-level-overview)
- [Lifecycle Layer](#lifecycle-layer)
- [Host Layer](#host-layer)
- [Registry Layer](#registry-layer)
- [Provider Layer](#provider-layer)
- [Service Layer](#service-layer)
- [Util Layer](#util-layer)
- [Service Implementations](#service-implementations)
- [Exception Hierarchy](#exception-hierarchy)
- [Sequence Diagrams](#sequence-diagrams)
  - [Startup Sequence](#startup-sequence)
  - [Shutdown Sequence](#shutdown-sequence)
  - [Cross-Thread Async Invocation](#cross-thread-async-invocation)

---

## High-Level Overview

A conceptual view of the five main framework layers and their dependencies.

```mermaid
classDiagram
    direction TB

    class Lifecycle["Lifecycle Layer"]
    class Host["Host Layer"]
    class Registry["Registry Layer"]
    class Provider["Provider Layer"]
    class Service["Service Layer"]

    Lifecycle --> Host : manages
    Lifecycle --> Registry : extracts registrations
    Host --> Provider : owns
    Host --> Service : creates & controls
    Provider --> Service : resolves
    Registry --> Service : stores factories
```

---

## Lifecycle Layer

The `LifecycleManager` orchestrates the entire service lifecycle, coordinating startup and shutdown across thread groups with priority ordering. `ExecutorContext` and `DispatchContext` provide thread-safe lifetime tracking and cross-thread operation support.

```mermaid
classDiagram
    direction TB

    class LifecycleManager {
        +StartServicesAsync() awaitable~void~
        +ShutdownServicesAsync() awaitable~void~
        +Update() ProcessResult
        +Poll() size_t
    }

    class LifecycleManagerConfig {
        <<struct>>
    }

    class ExecutorContext~T~ {
        +GetExecutor() any_io_executor
        +GetWeakPtr() weak_ptr~T~
        +TryLock() shared_ptr~T~
        +IsAlive() bool
    }

    class DispatchContext~T~ {
        +GetExecutor() any_io_executor
        +GetDispatcher() Dispatcher
        +GetWeakPtr() weak_ptr~T~
        +TryLock() shared_ptr~T~
        +IsAlive() bool
    }

    class CooperativeThreadHost
    class ServiceRegistry
    class ManagedThreadHost

    LifecycleManager --> LifecycleManagerConfig : configured by
    LifecycleManager --> ServiceRegistry : extracts registrations
    LifecycleManager --> CooperativeThreadHost : owns main thread host
    LifecycleManager --> "0..*" ManagedThreadHost : creates & manages
    CooperativeThreadHost --> ExecutorContext : provides
    ManagedThreadHost --> ExecutorContext : provides
```

---

## Host Layer

Service hosts manage thread execution and service lifecycle within their thread context.

```mermaid
classDiagram
    direction TB

    class ServiceHostBase {
        <<abstract>>
        +GetIoContext() io_context
        #ProcessServices() ProcessResult
        #DoTryStartServicesAsync() awaitable~void~
    }

    class CooperativeThreadHost {
        +SetWakeCallback(WakeCallback)
        +Poll() size_t
        +Update() ProcessResult
        +TryStartServicesAsync() awaitable~void~
    }

    class ManagedThreadServiceHost {
        +RunAsync() awaitable~void~
        +TryStartServicesAsync() awaitable~void~
    }

    class ManagedThreadHost {
        +StartAsync() awaitable~ManagedThreadRecord~
        +Stop()
        +GetIoContext() io_context
        +GetServiceHost() ServiceHostProxy
        +GetExecutorContext() ExecutorContext
    }

    class ServiceHostProxy {
        +TryStartServicesAsync() awaitable~void~
        +TryShutdownServicesAsync() vector~exception_ptr~
        +Update() ProcessResult
        +Poll() size_t
    }

    class IThreadSafeServiceHost {
        <<interface>>
        +TryStartServicesAsync() awaitable~void~
        +TryShutdownServicesAsync() vector~exception_ptr~
    }

    class ManagedThreadServiceProvider {
        +RegisterPriorityGroup()
        +UnregisterAllServices()
        +GetServiceCount() size_t
    }

    class ManagedThreadRecord

    CooperativeThreadHost o-- CooperativeThreadServiceHost : wraps
    ServiceHostBase <|-- CooperativeThreadServiceHost
    ServiceHostBase <|-- ManagedThreadServiceHost
    ServiceHostBase --> ManagedThreadServiceProvider : owns
    IThreadSafeServiceHost <|.. ServiceHostBase

    ManagedThreadHost *-- ManagedThreadServiceHost : contains
    ManagedThreadHost ..> ManagedThreadRecord : creates
    ServiceHostProxy --> IThreadSafeServiceHost : wraps
    ManagedThreadHost --> ServiceHostProxy : provides
```

---

## Registry Layer

The registry system handles service registration with priority and thread group assignment.

```mermaid
classDiagram
    direction LR

    class IServiceRegistry {
        <<interface>>
        +RegisterService(factory, priority, threadGroup)
        +CreateServiceThreadGroupId() ServiceThreadGroupId
        +GetMainServiceThreadGroupId() ServiceThreadGroupId
    }

    class ServiceRegistry {
        +RegisterService()
        +CreateServiceThreadGroupId()
        +GetMainServiceThreadGroupId()
        +ExtractRegistrations() vector~ServiceRegistrationRecord~
    }

    class ServiceRegistrationRecord {
        +Factory
        +Priority
        +ThreadGroupId
    }

    class ServiceLaunchPriority {
        +uint32_t Value
    }

    class ServiceThreadGroupId {
        +uint32_t Value
    }

    IServiceRegistry <|.. ServiceRegistry
    ServiceRegistry --> "*" ServiceRegistrationRecord : stores
    ServiceRegistrationRecord --> ServiceLaunchPriority
    ServiceRegistrationRecord --> ServiceThreadGroupId
```

---

## Provider Layer

The provider system enables dependency injection and service resolution.

```mermaid
classDiagram
    direction TB

    class IServiceProvider {
        <<interface>>
        +GetService(type_info) shared_ptr~IService~
        +TryGetService(type_info) shared_ptr~IService~
        +TryGetServices(type_info, vector) bool
    }

    class ServiceProvider {
        +GetService~T~() shared_ptr~T~
        +TryGetService~T~() shared_ptr~T~
        +TryGetServices~T~(vector) bool
    }

    class ServiceProviderProxy {
        +Clear()
        +GetService(type_info) shared_ptr~IService~
        +TryGetService(type_info) shared_ptr~IService~
    }

    class ManagedThreadServiceProvider {
        +RegisterPriorityGroup()
        +UnregisterAllServices()
        +GetAllServiceControls()
    }

    IServiceProvider <|.. ServiceProviderProxy
    IServiceProvider <|.. ManagedThreadServiceProvider
    ServiceProvider --> IServiceProvider : wraps
    ServiceProviderProxy --> IServiceProvider : wraps
```

---

## Service Layer

Core service abstractions defining the service contract and lifecycle.

```mermaid
classDiagram
    direction TB

    class IService {
        <<interface>>
    }

    class IServiceControl {
        <<interface>>
        +InitAsync(ServiceCreateInfo) awaitable~ServiceInitResult~
        +ShutdownAsync() awaitable~ServiceShutdownResult~
        +Process() ProcessResult
    }

    class ASyncServiceBase {
        <<abstract>>
    }

    class IServiceFactory {
        <<interface>>
        +GetSupportedInterfaces() span~type_info~
        +Create(type_info, ServiceCreateInfo) shared_ptr~IServiceControl~
    }

    class ProcessResult {
        +Status
        +SleepDuration
        +NoSleepLimit()$ ProcessResult
        +Quit()$ ProcessResult
        +SleepLimit(duration)$ ProcessResult
    }

    class ProcessStatus {
        <<enumeration>>
        NoSleepLimit
        SleepLimit
        Quit
    }

    class ServiceCreateInfo {
        +Provider
    }

    class ServiceInfo {
        +Name
    }

    class ServiceInitResult {
        <<enumeration>>
        UnknownError
        Success
    }

    class ServiceShutdownResult {
        <<enumeration>>
        UnknownError
        Success
    }

    IService <|-- IServiceControl
    IServiceControl <|-- ASyncServiceBase
    ProcessResult --> ProcessStatus
    IServiceFactory ..> IServiceControl : creates
    ServiceCreateInfo --> ServiceProvider : contains
```

---

## Util Layer

Utilities for cross-thread communication and safe async method invocation.

```mermaid
classDiagram
    direction TB

    class AsyncProxyHelper {
        <<utility>>
        +InvokeAsync(ExecutorContext, method, args)$ awaitable~Result~
        +TryInvokeAsync(ExecutorContext, method, args)$ awaitable~optional~Result~~
        +InvokeAsync(DispatchContext, method, args)$ awaitable~Result~
        +TryInvokeAsync(DispatchContext, method, args)$ awaitable~optional~Result~~
        +TryInvokePost(ExecutorContext, method, args)$ bool
        +DispatchInvokeAsync(DispatchContext, method, args)$ awaitable~Result~
    }

    class ExecutorContext~T~ {
        +GetExecutor() any_io_executor
        +TryLock() shared_ptr~T~
    }

    class DispatchContext~T~ {
        +GetExecutor() any_io_executor
        +GetDispatcher() Dispatcher
        +TryLock() shared_ptr~T~
    }

    class ServiceDisposedException

    AsyncProxyHelper ..> ExecutorContext : uses
    AsyncProxyHelper ..> DispatchContext : uses
    AsyncProxyHelper ..> ServiceDisposedException : throws when object disposed
```

---

## Service Implementations

Concrete service implementations demonstrating the dual inheritance pattern (extends `ASyncServiceBase` and implements service-specific interface).

```mermaid
classDiagram
    direction TB

    class IService {
        <<interface>>
    }

    class ASyncServiceBase {
        <<abstract>>
    }

    class IAddService {
        <<interface>>
        +AddAsync(a, b) awaitable~double~
    }

    class ISubtractService {
        <<interface>>
        +SubtractAsync(a, b) awaitable~double~
    }

    class IMultiplyService {
        <<interface>>
        +MultiplyAsync(a, b) awaitable~double~
    }

    class IDivideService {
        <<interface>>
        +DivideAsync(a, b) awaitable~double~
    }

    class ICalculatorService {
        <<interface>>
        +CalculateAsync(expression) awaitable~double~
    }

    class AddService
    class SubtractService
    class MultiplyService
    class DivideService
    class CalculatorService

    class AddServiceFactory
    class SubtractServiceFactory
    class MultiplyServiceFactory
    class DivideServiceFactory
    class CalculatorServiceFactory

    IService <|-- IAddService
    IService <|-- ISubtractService
    IService <|-- IMultiplyService
    IService <|-- IDivideService
    IService <|-- ICalculatorService

    ASyncServiceBase <|-- AddService
    ASyncServiceBase <|-- SubtractService
    ASyncServiceBase <|-- MultiplyService
    ASyncServiceBase <|-- DivideService
    ASyncServiceBase <|-- CalculatorService

    IAddService <|.. AddService
    ISubtractService <|.. SubtractService
    IMultiplyService <|.. MultiplyService
    IDivideService <|.. DivideService
    ICalculatorService <|.. CalculatorService

    AddServiceFactory ..> AddService : creates
    SubtractServiceFactory ..> SubtractService : creates
    MultiplyServiceFactory ..> MultiplyService : creates
    DivideServiceFactory ..> DivideService : creates
    CalculatorServiceFactory ..> CalculatorService : creates

    CalculatorService --> IAddService : uses
    CalculatorService --> ISubtractService : uses
    CalculatorService --> IMultiplyService : uses
    CalculatorService --> IDivideService : uses
```

---

## Exception Hierarchy

Custom exceptions organized by their standard library base class.

```mermaid
classDiagram
    direction TB

    class std_exception["std::exception"]

    class std_runtime_error["std::runtime_error"]
    class std_logic_error["std::logic_error"]
    class std_bad_cast["std::bad_cast"]

    class AggregateException
    class MultipleServicesFoundException
    class ServiceDisposedException
    class ServiceProviderException
    class UnknownServiceException
    class WrongThreadException

    class DuplicateServiceRegistrationException
    class EmptyPriorityGroupException
    class InvalidPriorityOrderException
    class InvalidServiceFactoryException
    class RegistryExtractedException

    class ServiceCastException

    std_exception <|-- std_runtime_error
    std_exception <|-- std_logic_error
    std_exception <|-- std_bad_cast

    std_runtime_error <|-- AggregateException
    std_runtime_error <|-- MultipleServicesFoundException
    std_runtime_error <|-- ServiceDisposedException
    std_runtime_error <|-- ServiceProviderException
    std_runtime_error <|-- UnknownServiceException
    std_runtime_error <|-- WrongThreadException

    std_logic_error <|-- DuplicateServiceRegistrationException
    std_logic_error <|-- EmptyPriorityGroupException
    std_logic_error <|-- InvalidPriorityOrderException
    std_logic_error <|-- InvalidServiceFactoryException
    std_logic_error <|-- RegistryExtractedException

    std_bad_cast <|-- ServiceCastException
```

---

## Sequence Diagrams

### Startup Sequence

High-level view of the `LifecycleManager` orchestrating service startup across multiple thread groups with priority ordering.

```mermaid
sequenceDiagram
    autonumber
    participant App as Application
    participant LM as LifecycleManager
    participant Reg as ServiceRegistry
    participant TH as ManagedThreadHosts
    participant Svc as Services

    App->>LM: StartServices()

    LM->>Reg: ExtractRegistrations()
    Reg-->>LM: vector<ServiceRegistrationRecord>

    Note over LM: Group by ThreadGroupId<br/>Sort by Priority (high→low)

    LM->>TH: Create ManagedThreadHost per ThreadGroup
    LM->>TH: StartAsync() for each host
    TH-->>LM: Threads running

    loop For each Priority Level (highest first)
        LM->>TH: TryStartServicesAsync(priority N services)

        par All Thread Groups
            TH->>Svc: Create service instances
            TH->>Svc: InitAsync()
            Svc-->>TH: ServiceInitResult
        end

        TH-->>LM: All priority N services ready
        Note over LM: Proceed to next lower priority
    end

    LM-->>App: All services started
```

### Shutdown Sequence

High-level view of the `LifecycleManager` orchestrating graceful shutdown in reverse priority order.

```mermaid
sequenceDiagram
    autonumber
    participant App as Application
    participant LM as LifecycleManager
    participant TH as ManagedThreadHosts
    participant Svc as Services

    App->>LM: ShutdownServices()

    loop For each Priority Level (lowest first)
        LM->>TH: Shutdown priority N services

        par All Thread Groups
            TH->>Svc: ShutdownAsync()
            Svc-->>TH: ServiceShutdownResult
        end

        TH-->>LM: All priority N services stopped
        Note over LM: Proceed to next higher priority
    end

    LM->>TH: Stop() for each host
    TH-->>LM: Threads terminated

    LM-->>App: Shutdown complete
```

### Cross-Thread Async Invocation

Demonstrates how `AsyncProxyHelper` uses `DispatchContext` to safely invoke async methods on objects living in different threads.

```mermaid
sequenceDiagram
    autonumber
    participant Caller as Calling Thread
    participant Helper as AsyncProxyHelper
    participant DC as DispatchContext<T>
    participant Exec as Target io_context
    participant Obj as Target Object

    Note over Caller: Want to call method on object<br/>running in different thread

    Caller->>Helper: InvokeAsync(dispatchCtx, &Method, args...)
    Helper->>DC: TryLock()
    DC-->>Helper: shared_ptr<T> (or nullptr)

    alt Object Still Alive
        Helper->>DC: GetDispatcher()
        DC-->>Helper: Dispatcher

        Note over Helper: Create async operation<br/>that will run on target thread

        Helper->>Exec: co_await dispatch(executor, use_awaitable)
        Note over Exec: Switch to target thread

        Exec->>Obj: Call method(args...)
        Obj-->>Exec: Return result

        Exec-->>Helper: Result
        Helper-->>Caller: co_return result

    else Object Disposed
        Helper-->>Caller: throw ServiceDisposedException
    end

    Note over Caller: Safe cross-thread call complete<br/>or disposed exception thrown
```

**Key Points:**

1. **Lifetime Safety**: `TryLock()` ensures object still exists before invocation
2. **Thread Switching**: `co_await dispatch()` moves execution to target thread's `io_context`
3. **Exception Handling**: Throws `ServiceDisposedException` if object was destroyed
4. **Type Safety**: Template methods maintain compile-time type checking
5. **Async-Friendly**: Fully compatible with C++20 coroutines (`co_await`)

**Alternative Patterns:**

- **TryInvokeAsync**: Returns `std::optional<Result>` instead of throwing on disposed objects
- **TryInvokePost**: Fire-and-forget posting without waiting for result
- **ExecutorContext**: Similar pattern but without dispatcher (simpler, dispatch-only)
- **DispatchInvokeAsync**: Uses `co_await dispatch()` explicitly for guaranteed thread context

---

## Design Patterns

The framework implements several well-known design patterns:

| Pattern | Implementation |
|---------|----------------|
| **Factory** | `IServiceFactory` creates service instances |
| **Service Locator** | `IServiceProvider` / `ServiceProvider` resolve dependencies |
| **Registry** | `ServiceRegistry` holds registrations before instantiation |
| **Proxy** | `ServiceProviderProxy` wraps provider with disconnect capability |
| **Template Method** | `ServiceHostBase` defines algorithm, subclasses provide specifics |
| **Value Object** | `ServiceLaunchPriority`, `ServiceThreadGroupId` wrap primitives |
| **Aggregate Exception** | `AggregateException` collects multiple failures |
