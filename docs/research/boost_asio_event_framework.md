# Thread-Safe Event Framework Using Boost.Asio

Since we already use Boost.Asio for request/response, we can leverage it for a thread-safe event system with cross-thread dispatch and thread-affinity control.

---

## Option 1: Use `boost::asio::io_context` as Your Event Bus
Each thread runs one `io_context.run()`. Any thread can post an event to any other thread’s context.

```cpp
boost::asio::io_context mainContext;
boost::asio::io_context workerContext;

void handleEventOnMain(int value)
{
    std::cout << "Handled on MAIN thread: " << value << "\n";
}

void handleEventOnWorker(int value)
{
    std::cout << "Handled on WORKER thread: " << value << "\n";
}

int main()
{
    std::thread mainThread([&] { mainContext.run(); });
    std::thread workerThread([&] { workerContext.run(); });

    // Emit from anywhere:
    boost::asio::post(mainContext,  []{ handleEventOnMain(42); });
    boost::asio::post(workerContext, []{ handleEventOnWorker(99); });

    mainThread.join();
    workerThread.join();
}
```

### Why this works
- Thread-safe: `post()` is safe from any thread.
- Fully asynchronous.
- Perfect control over which thread handles which event.
- No extra dependencies.

---

## Option 2: Use io_context executors to route events
You can pass around the executor of a particular thread/context:

```cpp
boost::asio::any_io_executor mainExec = mainContext.get_executor();
boost::asio::any_io_executor workerExec = workerContext.get_executor();

boost::asio::post(mainExec, [] { /* main thread handler */ });
boost::asio::post(workerExec, [] { /* worker thread handler */ });
```

This abstracts threads as executors for clean event routing.

---

## Option 3: Use `boost::asio::strand` for thread-safe handlers
A strand ensures serialized execution even with multiple threads running the same `io_context`.

```cpp
auto strand = boost::asio::make_strand(ioContext);

boost::asio::post(strand, []{
    // Executes serialized, thread-safe
});
```

---

## Option 4: Create a full event bus using Asio
Wrap the executor pattern into a reusable `EventBus`:

```cpp
class EventBus {
public:
    explicit EventBus(boost::asio::any_io_executor exec)
        : exec_(exec) {}

    template<typename Handler>
    void post(Handler&& h) {
        boost::asio::post(exec_, std::forward<Handler>(h));
    }

    template<typename Handler>
    void dispatch(Handler&& h) {
        boost::asio::dispatch(exec_, std::forward<Handler>(h));
    }

private:
    boost::asio::any_io_executor exec_;
};
```

Usage:

```cpp
EventBus mainBus(mainContext.get_executor());
EventBus workerBus(workerContext.get_executor());

// From any thread:
mainBus.post([]{ ... });
workerBus.post([]{ ... });
```

---

## Recommendation
Since we already use Boost.Asio:
- Use multiple `io_context` objects or executors to represent thread contexts.
- Use `post()`, `dispatch()`, or `strand` to route events safely and efficiently.

This integrates perfectly with existing Asio request/response code and provides a clean, reusable, thread-safe event framework.

