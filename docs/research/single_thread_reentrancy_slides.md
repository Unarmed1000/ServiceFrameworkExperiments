# Slide 1 --- The Single-Threaded Re-Entrancy Problem

-   Re-entrancy happens when a function is called again *before the
    previous call finishes*
-   This can occur even on a **single thread**
-   Common cause: **callbacks executed immediately during an operation**
-   Leads to:
    -   Broken invariants\
    -   State corruption\
    -   Iterator invalidation\
    -   Undefined behavior

# Slide 2 --- Why Single-Threaded Re-Entrancy Is Dangerous

Even though only one thread runs at a time:

-   Callbacks can fire at **unexpected points**
-   The system may be in a **half-updated state**
-   The callback may:
    -   Modify a collection currently being iterated\
    -   Re-enter the same function\
    -   Violate assumptions the caller relies on\
-   Debugging becomes extremely difficult

# Slide 3 --- A Classic Example of Single-Threaded Re-Entrancy

### Looping a vector with an iterator

**inside the loop, a callback erases an element**

``` cpp
std::vector<int> v = {1,2,3,4,5};

void callback(std::vector<int>& v) {
    v.erase(std::find(v.begin(), v.end(), 3));  // invalidates iterators
}

void call_with_callback(std::vector<int>& v) {
    callback(v);
}

for (auto it = v.begin(); it != v.end(); ++it) {
    call_with_callback(v);   // re-entrant modification
    std::cout << *it;        // UB: iterator now invalid
}
```

→ **Undefined behavior caused by single-threaded re-entrancy**

# Slide 4 --- How to Prevent the Single-Threaded Re-Entrancy Problem

### Avoid executing callbacks immediately

Callbacks invoked *inside* critical sections are the #1 cause of
re-entrancy issues.

# Slide 5 --- Queue Callbacks Instead of Executing Them Immediately

### Why queue?

-   Prevents accidental re-entry\
-   Allows current operation to complete with valid invariants\
-   Avoids modifying data structures while they're being iterated\
-   Keeps execution order predictable\
-   Matches well-established event loop patterns (JS, Qt, UI apps, game
    engines)

# Slide 6 --- Why Queuing Solves Single-Threaded Re-Entrancy

### Without Queueing

    function A()
      modify data
      fire callback → re-enters A → corrupts state
      resume A()

### With Queueing

    function A()
      modify data
      schedule callback
      finish A()
    process queued callbacks safely

# Slide 7 --- Summary

### **The Single-Threaded Re-Entrancy Problem**

-   Happens when callbacks run immediately\
-   Can cause state corruption, UB, and invalid iterators\
-   Hard to diagnose when deeply nested\
-   **Solution: Queue callbacks** and process after the current
    operation finishes\
-   Ensures safety, predictability, and sane control flow
