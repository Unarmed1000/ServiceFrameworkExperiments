# Service Threading Types

## 1. Free-threaded / Global Thread-safe Services
- **Definition:** Can be called from any thread concurrently; internal state is safe for multithreaded access.
- **Terminology:**
  - Free-threaded service (COM term)
  - Thread-safe service
  - Concurrent service
- **Examples:** Logging service with proper locking, a thread-safe cache, a global configuration service.

---

## 2. Single-threaded / Thread-affine Services
- **Definition:** Must be called from a specific thread (often the one that created it); calls from other threads are invalid.
- **Terminology:**
  - Single-threaded service
  - Thread-bound service
  - Thread-affine service
  - STA service (COM term)
  - UI-thread-only (common in GUI frameworks)
- **Examples:** WPF/WinForms UI elements, COM STA objects.

---

## 3. Thread-redirected / Marshaled Services
- **Definition:** Can be called from any thread, but operations are marshaled to the host thread for execution. Callers don’t need to worry about thread safety; the service enforces it internally.
- **Terminology:**
  - Thread-affine with marshalling
  - Dispatcher-based service (common in .NET)
  - Serialized service / Sequential service
  - Actor-style service (in actor-model systems)
- **Examples:**
  - WPF `Dispatcher.Invoke`-based services
  - Game engine main-thread services that accept requests from worker threads
  - Actor mailbox services

---

## Summary Table

| Service Type | Can be called from any thread? | Executes on | Notes / Terms |
|--------------|-------------------------------|------------|----------------|
| Free-threaded | ✅ | Any thread | Free-threaded, Thread-safe, Concurrent |
| Single-threaded | ❌ | Specific thread only | Thread-affine, Thread-bound, STA, UI-thread-only |
| Redirected / Marshaled | ✅ | Host thread | Thread-affine w/ marshalling, Dispatcher-based, Actor-style |

