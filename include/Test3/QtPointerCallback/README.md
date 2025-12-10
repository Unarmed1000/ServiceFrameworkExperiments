# Qt Pointer Callback - Boost.Asio Coroutine Integration

Lightweight Qt5-based async helper for executing `boost::asio::awaitable<T>` coroutines with automatic lifetime safety using QPointer, without requiring Qt's MOC or slot declarations.

## Overview

This implementation provides simple integration between Boost.Asio coroutines and QObject callbacks without MOC dependencies. It executes async operations on a Boost.Asio executor and marshals callbacks back to QObject member functions using `Qt::QueuedConnection`, with automatic lifetime safety via QPointer.

**Key Features:**
- ✅ No Q_OBJECT macro required - works without MOC
- ✅ Regular member functions as callbacks (no slots needed)
- ✅ Automatic lifetime safety via QPointer null checks
- ✅ Thread-safe callback marshaling using Qt's queued connections
- ✅ Exception propagation through `std::future<T>`
- ✅ Support for both void and non-void result types
- ✅ Header-only friendly design
- ✅ Minimal overhead compared to slot-based approach

## Architecture

### Class Diagram

```mermaid
classDiagram
    class QtPointerCallbackReceiver {
        +QtPointerCallbackReceiver(executor, parent)
        +GetExecutor() any_io_executor
        +CallAsync(method, lambda) future~T~
        -m_executor any_io_executor
    }

    class QObject {
        <<Qt>>
        +destroyed() signal
    }

    class QPointer~T~ {
        <<Qt>>
        +isNull() bool
        +data() T*
    }

    class ExampleServiceUse {
        +DoSimpleWork()
        +DoWorkThatMightFail(shouldFail)
        +DoVoidWork()
        +DoMultipleOperations()
        -HandleDoubleResult(future)
        -HandleVoidResult(future)
        -m_operationCount int
    }

    QtPointerCallbackReceiver --|> QObject
    ExampleServiceUse --|> QtPointerCallbackReceiver
    QPointer~T~ ..> "protects" QObject

    note for QtPointerCallbackReceiver "NO Q_OBJECT macro needed\nCallbacks are regular methods"
    note for QPointer~T~ "Automatic null check\nprevents use-after-free"
```

### Component Interaction

```mermaid
graph LR
    A[Client Code] --> B[QtPointerCallbackReceiver]
    B --> C[ToFutureWithCallback]
    C --> D[QPointer Creation]
    C --> E[boost::asio::co_spawn]
    C --> F[QMetaObject::invokeMethod]
    E --> G[Boost.Asio Executor]
    F --> H[QPointer Null Check]
    H --> I{Object Alive?}
    I -->|Yes| J[Qt Event Loop]
    I -->|No| K[Skip Callback]
    J --> L[Member Function]

    style B fill:#e1f5ff
    style C fill:#fff4e1
    style D fill:#ffe1f5
    style G fill:#ffe1e1
    style J fill:#e1ffe1
    style K fill:#ffcccc
```

## Threading Model

### Thread Boundaries

```mermaid
sequenceDiagram
    participant Client as Client Thread
    participant QtLoop as Qt Main Thread
    participant Executor as Boost.Asio Executor
    participant Coroutine as Async Coroutine

    Note over Client: Any thread can initiate
    Client->>QtLoop: CallAsync(method, lambda)
    QtLoop->>Executor: co_spawn(lambda)

    Note over Executor: Coroutine executes here
    Executor->>Coroutine: Execute awaitable<T>
    Coroutine->>Coroutine: Async work...
    Coroutine-->>Executor: Result or Exception

    Note over Executor: Capture in promise
    Executor->>Executor: Check QPointer

    alt QPointer is null
        Note over Executor: Object destroyed
        Executor->>Executor: Skip callback (safe)
    else QPointer is valid
        Executor->>QtLoop: QMetaObject::invokeMethod(..., Qt::QueuedConnection)
        Note over QtLoop: Double-check QPointer
        QtLoop->>QtLoop: Invoke method(future<T>)
        QtLoop->>Client: Future can be accessed
    end
```

## Sequence Diagrams

### Normal Flow - Successful Completion

```mermaid
sequenceDiagram
    participant App as Application
    participant Receiver as QtPointerCallbackReceiver
    participant ToFuture as ToFutureWithCallback
    participant QPtr as QPointer
    participant CoSpawn as boost::asio::co_spawn
    participant Qt as QMetaObject::invokeMethod
    participant Method as Callback Method

    App->>Receiver: CallAsync(method, lambda)
    Receiver->>ToFuture: ToFutureWithCallback(executor, this, method, lambda)
    ToFuture->>ToFuture: Create promise & future
    ToFuture->>QPtr: Create QPointer(this)
    ToFuture->>CoSpawn: Spawn coroutine on executor
    ToFuture-->>Receiver: Return future<T>
    Receiver-->>App: Return future<T>

    Note over CoSpawn: Async execution on executor thread
    CoSpawn->>CoSpawn: co_await coroutineLambda()
    CoSpawn->>CoSpawn: promise->set_value(result)
    CoSpawn->>QPtr: Check isNull()
    QPtr-->>CoSpawn: false (object alive)
    CoSpawn->>Qt: invokeMethod(method, Qt::QueuedConnection)

    Note over Qt: Marshaled to Qt thread
    Qt->>QPtr: Check isNull() again
    QPtr-->>Qt: false (still alive)
    Qt->>Method: Invoke method(future<T>)
    Method->>Method: future.get() → result
    Method->>App: Update UI / Process result
```

### Lifetime Safety Flow - Object Destroyed

```mermaid
sequenceDiagram
    participant App as Application
    participant Receiver as QtPointerCallbackReceiver
    participant QPtr as QPointer
    participant CoSpawn as Coroutine
    participant Method as Callback Method

    App->>Receiver: CallAsync(method, lambda)
    Receiver->>QPtr: Create QPointer(this)
    Receiver-->>App: Return future<T>

    Note over CoSpawn: Async work in progress
    CoSpawn->>CoSpawn: Executing coroutine...

    Note over App: Object destroyed before completion
    App->>Receiver: delete / ~QtPointerCallbackReceiver()
    Receiver->>QPtr: QObject destroyed
    QPtr->>QPtr: Set internal pointer to null

    Note over CoSpawn: Coroutine completes
    CoSpawn->>CoSpawn: Result ready
    CoSpawn->>QPtr: Check isNull()
    QPtr-->>CoSpawn: true (object destroyed)
    CoSpawn->>CoSpawn: Skip invokeMethod

    Note over Method: ✅ Callback NOT invoked (safe)
    Note over App: No use-after-free, no crash
```

### Error Flow - Exception Handling

```mermaid
sequenceDiagram
    participant App as Application
    participant Receiver as QtPointerCallbackReceiver
    participant CoSpawn as Coroutine
    participant Method as Callback Method

    App->>Receiver: CallAsync(method, lambda)
    Receiver-->>App: Return future<T>

    Note over CoSpawn: Exception thrown
    CoSpawn->>CoSpawn: try { co_await lambda() }
    CoSpawn->>CoSpawn: throw std::exception
    CoSpawn->>CoSpawn: catch { promise->set_exception() }
    CoSpawn->>CoSpawn: Check QPointer (not null)
    CoSpawn->>Method: invokeMethod(method)

    Note over Method: Exception retrieved
    Method->>Method: try { future.get() }
    Method->>Method: catch (exception) { handle error }
    Method->>App: Show error message
```

### Multiple Concurrent Operations

```mermaid
sequenceDiagram
    participant App as Application
    participant Receiver as QtPointerCallbackReceiver
    participant Executor as Boost.Asio Executor
    participant Method as Callback Method

    Note over App: Launch 5 concurrent operations
    loop 5 times
        App->>Receiver: CallAsync(method, lambda[i])
        Receiver->>Executor: co_spawn(lambda[i])
    end

    Note over Executor: All coroutines execute concurrently
    par Operation 0
        Executor->>Executor: Execute lambda[0]
        Executor->>Method: Callback(future<0>)
    and Operation 1
        Executor->>Executor: Execute lambda[1]
        Executor->>Method: Callback(future<1>)
    and Operation 2
        Executor->>Executor: Execute lambda[2]
        Executor->>Method: Callback(future<2>)
    and Operation 3
        Executor->>Executor: Execute lambda[3]
        Executor->>Method: Callback(future<3>)
    and Operation 4
        Executor->>Executor: Execute lambda[4]
        Executor->>Method: Callback(future<4>)
    end

    Note over Method: All callbacks invoked on Qt thread
    Method->>App: All operations complete
```

### QPointer Protection Mechanism

```mermaid
sequenceDiagram
    participant Coroutine as Async Coroutine
    participant QPtr as QPointer<QObject>
    participant QObject as Target Object
    participant Callback as Callback Invocation

    Note over Coroutine: Coroutine completes
    Coroutine->>QPtr: First null check (before invokeMethod)

    alt Object was destroyed
        QPtr-->>Coroutine: isNull() = true
        Note over Coroutine: Skip invokeMethod entirely
        Coroutine->>Coroutine: Return early ✅
    else Object still alive
        QPtr-->>Coroutine: isNull() = false
        Coroutine->>Callback: QMetaObject::invokeMethod(...)
        Note over Callback: Queued on Qt event loop

        Note over QObject: Time passes...

        alt Object destroyed before callback executes
            QObject->>QPtr: Object destroyed
            QPtr->>QPtr: Internal pointer → null
            Callback->>QPtr: Second null check (in lambda)
            QPtr-->>Callback: isNull() = true
            Note over Callback: Skip method invocation ✅
        else Object still alive
            Callback->>QPtr: Second null check (in lambda)
            QPtr-->>Callback: isNull() = false
            Callback->>QObject: Invoke callback method ✅
        end
    end
```

## API Reference

### ToFutureWithCallback

```cpp
template <typename TCallback, typename CallbackMethod, typename CoroutineLambda>
auto ToFutureWithCallback(
    boost::asio::any_io_executor workExecutor,
    TCallback* callbackObj,
    CallbackMethod callbackMethod,
    CoroutineLambda coroutineLambda
) -> std::future<T>
```

**Parameters:**
- `workExecutor`: Boost.Asio executor to run the coroutine on
- `callbackObj`: Pointer to QObject with callback method (must inherit from QObject)
- `callbackMethod`: Pointer to callback member function (e.g., `&MyClass::handleResult`)
- `coroutineLambda`: Lambda returning `boost::asio::awaitable<T>`

**Returns:** `std::future<T>` for result access

**Requirements:**
- `TCallback` must inherit from `QObject`
- `callbackMethod` is a regular member function (NOT a slot)
- `coroutineLambda` must return `boost::asio::awaitable<T>`
- NO Q_OBJECT macro required
- NO MOC processing needed

**Lifetime Safety:**
- Uses `QPointer<TCallback>` to track object lifetime
- Two null checks: before `invokeMethod` and in the queued lambda
- Callback automatically skipped if object destroyed

### QtPointerCallbackReceiver

```cpp
class QtPointerCallbackReceiver : public QObject
{
public:
    explicit QtPointerCallbackReceiver(
        boost::asio::any_io_executor executor,
        QObject* parent = nullptr
    );

    boost::asio::any_io_executor GetExecutor() const;

    template <typename CallbackMethod, typename CoroutineLambda>
    auto CallAsync(CallbackMethod method, CoroutineLambda lambda);
};
```

**Methods:**
- `CallAsync`: Convenience wrapper that automatically passes `this` and stored executor

**Benefits:**
- Automatically passes `this` and stored executor
- Cleaner syntax at call sites
- Consistent executor management
- NO Q_OBJECT macro in this class

**Note:** Derived classes do NOT need Q_OBJECT unless they want to use Qt signals/slots.

## Usage Examples

### Example 1: Simple Async Call Without MOC

```cpp
// No Q_OBJECT needed!
class MyWidget : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    MyWidget(boost::asio::any_io_executor executor)
        : QtPointerCallbackReceiver(executor)
    {
    }

    void loadData()
    {
        // Fire and forget - callback will be invoked on completion
        CallAsync(&MyWidget::onDataLoaded,
                  []() -> boost::asio::awaitable<QString>
                  {
                      // Simulate async database query
                      co_return QString("Database result");
                  });
    }

private:
    // Regular member function - NOT a slot!
    void onDataLoaded(std::future<QString> future)
    {
        try
        {
            QString data = future.get();
            // Update UI safely on Qt thread
            // label->setText(data);
        }
        catch (const std::exception& ex)
        {
            // Handle error
            // QMessageBox::warning(nullptr, "Error", ex.what());
        }
    }
};
```

### Example 2: Automatic Lifetime Safety

```cpp
class DataProcessor : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    DataProcessor(boost::asio::any_io_executor executor)
        : QtPointerCallbackReceiver(executor)
    {
    }

    void processLongRunningTask()
    {
        CallAsync(&DataProcessor::onProcessingComplete,
                  []() -> boost::asio::awaitable<ProcessResult>
                  {
                      // Simulate very long processing (e.g., 10 seconds)
                      ProcessResult result;
                      result.success = true;
                      co_return result;
                  });

        // Safe to delete this object immediately!
        // If deleted before coroutine completes, callback is automatically skipped
        // No crash, no use-after-free
    }

private:
    void onProcessingComplete(std::future<ProcessResult> future)
    {
        // This method is only called if object is still alive
        try
        {
            ProcessResult result = future.get();
            // Process result
        }
        catch (const std::exception& ex)
        {
            // Handle error
        }
    }
};

// Usage:
auto processor = new DataProcessor(executor);
processor->processLongRunningTask();
delete processor;  // Safe! Callback will be skipped via QPointer check
```

### Example 3: Error Handling

```cpp
class ApiClient : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    void fetchUserData(int userId)
    {
        CallAsync(&ApiClient::onUserDataReceived,
                  [userId]() -> boost::asio::awaitable<UserData>
                  {
                      if (userId < 0)
                      {
                          throw std::invalid_argument("Invalid user ID");
                      }

                      // Simulate API call
                      UserData data;
                      data.id = userId;
                      data.name = "John Doe";
                      co_return data;
                  });
    }

private:
    void onUserDataReceived(std::future<UserData> future)
    {
        try
        {
            UserData data = future.get();
            // Update UI with user data
            // updateUserDisplay(data);
        }
        catch (const std::invalid_argument& ex)
        {
            // Handle validation error
            // showError("Validation failed: " + QString(ex.what()));
        }
        catch (const std::exception& ex)
        {
            // Handle other errors
            // showError("API error: " + QString(ex.what()));
        }
    }
};
```

### Example 4: Void Operations

```cpp
class Logger : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    void saveLogAsync(const QString& message)
    {
        CallAsync(&Logger::onLogSaved,
                  [message]() -> boost::asio::awaitable<void>
                  {
                      // Simulate async file write
                      // writeToFile(message);
                      co_return;
                  });
    }

private:
    void onLogSaved(std::future<void> future)
    {
        try
        {
            future.get();  // Check for exceptions
            // statusBar->showMessage("Log saved successfully");
        }
        catch (const std::exception& ex)
        {
            // statusBar->showMessage("Failed to save log: " + QString(ex.what()));
        }
    }
};
```

### Example 5: Multiple Concurrent Operations

```cpp
class BatchProcessor : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    void processBatch(const QVector<QString>& items)
    {
        m_totalItems = items.size();
        m_completedItems = 0;

        for (const QString& item : items)
        {
            CallAsync(&BatchProcessor::onItemProcessed,
                      [item]() -> boost::asio::awaitable<ProcessResult>
                      {
                          // Process each item concurrently
                          ProcessResult result;
                          result.item = item;
                          result.success = true;
                          co_return result;
                      });
        }
    }

private:
    void onItemProcessed(std::future<ProcessResult> future)
    {
        try
        {
            ProcessResult result = future.get();
            ++m_completedItems;

            // Update progress
            // progressBar->setValue(m_completedItems * 100 / m_totalItems);

            if (m_completedItems == m_totalItems)
            {
                // All items processed
                // If this class had Q_OBJECT: emit batchComplete();
            }
        }
        catch (const std::exception& ex)
        {
            // Handle error
            // If this class had Q_OBJECT: emit itemFailed(ex.what());
        }
    }

private:
    int m_totalItems;
    int m_completedItems;
};
```

### Example 6: With Q_OBJECT for Signals (Optional)

```cpp
// You CAN still use Q_OBJECT if you want signals/slots in your derived class
class Downloader : public Test3::QtPointer::QtPointerCallbackReceiver
{
    Q_OBJECT  // Optional - only if you want signals/slots

public:
    Downloader(boost::asio::any_io_executor executor)
        : QtPointerCallbackReceiver(executor)
    {
    }

    void download(const QUrl& url)
    {
        CallAsync(&Downloader::onDownloadComplete,
                  [url]() -> boost::asio::awaitable<QByteArray>
                  {
                      // Simulate download
                      co_return QByteArray("Downloaded data");
                  });
    }

signals:
    void downloadFinished(const QByteArray& data);
    void downloadFailed(const QString& error);

private:
    void onDownloadComplete(std::future<QByteArray> future)
    {
        try
        {
            QByteArray data = future.get();
            emit downloadFinished(data);
        }
        catch (const std::exception& ex)
        {
            emit downloadFailed(ex.what());
        }
    }
};
```

### Example 7: Using Future for Synchronous Wait

```cpp
class DataValidator : public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    bool validateDataSync(const QString& data)
    {
        // Get future for synchronous wait
        auto future = CallAsync(&DataValidator::onValidationComplete,
                                [data]() -> boost::asio::awaitable<bool>
                                {
                                    // Async validation logic
                                    co_return !data.isEmpty();
                                });

        // Block and wait for result (callback also invoked if object alive)
        try
        {
            return future.get();
        }
        catch (...)
        {
            return false;
        }
    }

private:
    void onValidationComplete(std::future<bool> future)
    {
        try
        {
            bool valid = future.get();
            // Handle result asynchronously
        }
        catch (const std::exception& ex)
        {
            // Handle error
        }
    }
};
```

### Example 8: Standalone Function Usage (Without Base Class)

```cpp
// You don't have to inherit from QtPointerCallbackReceiver
class MyComponent : public QObject
{
public:
    MyComponent(boost::asio::any_io_executor executor)
        : m_executor(executor)
    {
    }

    void doWork()
    {
        // Call standalone function directly
        auto future = Test3::QtPointer::ToFutureWithCallback(
            m_executor,
            this,
            &MyComponent::handleResult,
            []() -> boost::asio::awaitable<int>
            {
                co_return 42;
            });
    }

private:
    void handleResult(std::future<int> future)
    {
        try
        {
            int result = future.get();
            // Process result
        }
        catch (const std::exception& ex)
        {
            // Handle error
        }
    }

    boost::asio::any_io_executor m_executor;
};
```

## No MOC Required - Build Setup

### CMake Configuration

```cmake
# NO CMAKE_AUTOMOC needed for QtPointerCallback!
# (Unless your derived classes use Q_OBJECT for signals/slots)

# Find Qt5
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Your target
add_executable(MyApp
    main.cpp
    mywidget.cpp
    # No .h files needed for MOC processing
)

target_link_libraries(MyApp
    Qt5::Core
    Qt5::Widgets
    Boost::system
    # ... other dependencies
)

# Optional: Only enable AUTOMOC if you use Q_OBJECT in your classes
# set(CMAKE_AUTOMOC ON)
```

### qmake Configuration

```pro
QT += core widgets
CONFIG += c++20

# No MOC processing needed for QtPointerCallback base class

SOURCES += \
    main.cpp \
    mywidget.cpp

# Headers don't need to be listed for MOC
# (unless they contain Q_OBJECT)

LIBS += -lboost_system
```

### Header Include Order

```cpp
// Simple - no special MOC requirements
#include "QtPointerCallbackReceiver.hpp"
#include <QWidget>

// No Q_OBJECT needed
class MyWidget : public QWidget, public Test3::QtPointer::QtPointerCallbackReceiver
{
public:
    // Regular member functions as callbacks - no slots keyword
    void handleResult(std::future<int> future);
};
```

## When to Use This Implementation

### ✅ Use QtPointer When:

- You want to avoid MOC dependencies
- You're using Qt primarily for QObject lifetime management, not signals/slots
- You need minimal build complexity
- You prefer simpler, more portable code
- You're integrating with existing non-MOC QObject code
- You value automatic lifetime safety over explicit cancellation
- Your project uses header-only libraries
- You want the lightest-weight Qt integration

### ⚠️ Consider Alternatives When:

- You need explicit callback cancellation (use QtSlot implementation)
- You want Qt-native signal/slot integration
- You're already using MOC extensively in your project
- You want connection tracking capabilities
- You prefer Qt idioms over simplicity

## Performance Considerations

### QPointer Overhead

**Null Check Cost:**
- Two QPointer null checks per callback (very lightweight)
- `isNull()` is a simple pointer comparison
- Negligible overhead (nanoseconds on modern CPUs)

**Memory Overhead:**
- QPointer internally uses Qt's weak pointer mechanism
- Small per-instance overhead (~8 bytes on 64-bit)
- No heap allocation for QPointer itself

### Comparison with QtSlot

| Metric | QtPointer | QtSlot |
|--------|-----------|---------|
| **Null checks** | 2 per callback | 0 |
| **Connection tracking** | No | Yes (small overhead) |
| **MOC processing** | No | Yes |
| **Typical overhead** | ~10-20ns | ~20-30ns |
| **Memory per operation** | QPointer (~8 bytes) | Connection (~16 bytes) |

**Recommendation:** Both implementations have negligible overhead for typical async operations (milliseconds+). Use QtPointer for simplicity unless you need explicit cancellation.

### Qt Queued Connection Overhead

- Same as QtSlot implementation
- Uses Qt's event queue
- Small latency (typically microseconds)
- Negligible for UI updates and typical async operations

## Troubleshooting

### Callback Not Invoked

**Problem:** Callback method never called

**Possible Causes:**
1. Object destroyed before coroutine completed (expected behavior - QPointer protection)
2. Wrong thread for QObject
3. Qt event loop not running

**Solutions:**
1. Check object lifetime:
```cpp
qDebug() << "Object still alive:" << (object != nullptr);
```

2. Verify thread affinity:
```cpp
qDebug() << "Object thread:" << thread();
qDebug() << "Current thread:" << QThread::currentThread();
```

3. Ensure Qt event loop is running:
```cpp
QCoreApplication::exec();  // Or QApplication::exec()
```

### Compiler Error: "QPointer not found"

**Problem:** Missing Qt includes

**Solution:**
```cpp
#include <QPointer>
#include <QObject>
```

### Compiler Error: "awaitable<T> not found"

**Problem:** Missing Boost.Asio includes

**Solution:**
```cpp
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
```

### QPointer isNull() Always True

**Problem:** Object destroyed immediately

**Cause:** Object goes out of scope or is deleted too quickly

**Solution:**
```cpp
// Bad - object destroyed immediately
{
    MyComponent component(executor);
    component.doWork();
}  // component destroyed here

// Good - object lifetime managed properly
auto component = new MyComponent(executor);
component->setParent(someQObject);  // Or manage lifetime explicitly
component->doWork();
```

### Callback Invoked on Wrong Thread

**Problem:** Expected callback on main thread, but got worker thread

**Cause:** Qt object moved to different thread

**Solution:**
```cpp
// Ensure object is on the correct thread
myObject->moveToThread(QApplication::instance()->thread());
```

## Limitations

1. **No Explicit Cancellation**: Cannot disconnect callbacks before completion (use QtSlot for this)
2. **QPointer Overhead**: Small overhead for null checking (though negligible)
3. **Silent Failures**: Callbacks silently skipped if object destroyed (by design for safety)
4. **QObject Dependency**: Still requires QObject base (lighter than full Qt MOC though)
5. **No Connection Tracking**: Cannot query if callbacks are pending

## Comparison with QtSlot Implementation

| Feature | QtPointer (This) | QtSlot |
|---------|------------------|---------|
| **MOC Required** | ❌ No | ✅ Yes |
| **Callback Type** | Regular member functions | Qt slots |
| **Connection Tracking** | ❌ Not available | ✅ Built-in |
| **Explicit Cancellation** | ❌ No (relies on destruction) | ✅ Via disconnect |
| **Build Complexity** | ✅ Lower (no MOC) | Higher (requires MOC) |
| **Qt Integration** | Basic (QObject only) | Native (signals/slots) |
| **Overhead** | ✅ Minimal (QPointer check) | Small (connection tracking) |
| **Lifetime Safety** | ✅ Automatic (QPointer) | ✅ Automatic (Qt cleanup) |
| **Header-Only Friendly** | ✅ Yes | ❌ No (needs MOC) |
| **Portability** | ✅ Higher | Lower (Qt build system) |
| **Qt Idioms** | Less Qt-specific | ✅ Follows Qt patterns |

### When to Choose Which

```mermaid
graph TD
    A[Need Qt async integration?] -->|Yes| B{Need explicit cancellation?}
    B -->|Yes| C[Use QtSlot]
    B -->|No| D{Already using MOC?}
    D -->|Yes| E{Prefer Qt idioms?}
    E -->|Yes| C
    E -->|No| F[Use QtPointer]
    D -->|No| F

    style C fill:#e1f5ff
    style F fill:#e1ffe1
```

## See Also

- `Test3::QtSlot` - Slot-based implementation with connection tracking and explicit cancellation
- `Test3::AwaitableToFuture` - Original Boost.Asio-only implementation with stop_token
- Qt Documentation: [QPointer](https://doc.qt.io/qt-5/qpointer.html)
- Qt Documentation: [QObject](https://doc.qt.io/qt-5/qobject.html)
- Qt Documentation: [Thread Support](https://doc.qt.io/qt-5/threads-qobject.html)
