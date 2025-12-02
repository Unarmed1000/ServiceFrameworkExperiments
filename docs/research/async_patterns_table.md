# Async Patterns Classification

| **Main Concept** | **Sub-Idea / Pattern** | **Description** | **C# Example** | **C++ Example** | **Key Characteristics** |
|-----------------|-----------------------|----------------|----------------|----------------|------------------------|
| **Request/Response** | Simple Request/Response | One request → one response | `Task<T>` / `async`-`await` | `std::future<T>` / `std::async` | Single completion, error propagation |
|                 | Continuation / Callback | Request → response → chained handling | `Task.ContinueWith(...)` | `future.then(...)` (library) | Deferred handling, chainable, still single response |
|                 | Fire-and-Forget | Request with discarded response | `Task.Run(() => DoWork())` | `std::thread([](){...}).detach()` | No result observed, side-effect only, unobserved errors |
| **Asynchronous Messaging** | Event / Publish-Subscribe | Broadcast request → many responses | `event EventHandler<T>` / `IObservable<T>` | Callback lists, Qt signals/slots | One-to-many, decoupled, unordered by default |
|                 | Producer-Consumer / Queue | Streamed or batched requests → responses | `Channel<T>` / `BlockingCollection<T>` | `std::queue<T> + mutex + condition_variable` | Many-to-many, asynchronous streaming, in-memory only |
|                 | Message Queue | Persistent or distributed producer-consumer | `RabbitMQ`, `Azure Service Bus`, `System.Threading.Channels` | `ZeroMQ`, `Boost.Asio` queue | Cross-process/machine, reliable delivery, persistent or durable, decoupled producers/consumers |

