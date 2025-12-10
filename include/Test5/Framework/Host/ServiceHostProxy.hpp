#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_HOST_SERVICEHOSTPROXY_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_HOST_SERVICEHOSTPROXY_HPP
//****************************************************************************************************************************************************
//* Zero-Clause BSD (0BSD)
//*
//* Copyright (c) 2025, Mana Battery
//*
//* Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
//*
//* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
//* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//****************************************************************************************************************************************************

#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/thread/future.hpp>
#include <exception>
#include <memory>
#include <vector>

namespace Test5
{
  class ServiceHostBase;

  /// @brief Thread-safe proxy for a ServiceHostBase running on another thread.
  ///
  /// This proxy can be safely used from any thread to invoke operations on a
  /// service host that lives on a different thread. All operations are marshalled
  /// to the target executor and return boost::future for the result.
  ///
  /// boost::future supports .then() for non-blocking continuations:
  ///
  /// @code
  /// // Non-blocking with continuation:
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// future.then([](boost::future<void> f) {
  ///     try { f.get(); }
  ///     catch (...) { /* handle error */ }
  /// });
  ///
  /// // Blocking:
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// future.get();  // Blocks until complete, throws on error
  ///
  /// // With lifetime-safe callback helpers:
  /// #include <Test5/Framework/Util/StopTokenCallbackHelper.hpp>
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// future.then(CallbackHelper::CreateCallback<MyClass, decltype(&MyClass::OnComplete), void>(
  ///     executor, this, &MyClass::OnComplete, stopToken));
  /// @endcode
  class ServiceHostProxy final
  {
    ///! Dispatch context containing source and target executor contexts.
    Test2::DispatchContext<Test2::ILifeTracker, ServiceHostBase> m_dispatchContext;

  public:
    /// @brief Constructs a proxy that marshals operations to the given service host.
    /// @param dispatchContext Dispatch context containing source and target executor contexts.
    explicit ServiceHostProxy(Test2::DispatchContext<Test2::ILifeTracker, ServiceHostBase> dispatchContext);
    ~ServiceHostProxy();

    // Non-copyable, non-movable
    ServiceHostProxy(const ServiceHostProxy&) = delete;
    ServiceHostProxy& operator=(const ServiceHostProxy&) = delete;
    ServiceHostProxy(ServiceHostProxy&&) = delete;
    ServiceHostProxy& operator=(ServiceHostProxy&&) = delete;

    /// @brief Asynchronously starts services with the specified launch priority.
    ///
    /// This method marshals the start request to the service host's executor.
    /// The operation will fail gracefully if the service host has been destroyed.
    ///
    /// @param services Vector of service records to start.
    /// @param currentPriority The launch priority for this batch of services.
    /// @return A boost::future that completes when all services have been started.
    ///         Exceptions during startup are propagated through the future.
    ///         Use .then() for non-blocking continuation or .get() to block.
    /// @note This method is safe to call from any thread. The actual startup logic
    ///       executes on the service host's executor.
    boost::future<void> TryStartServicesAsync(std::vector<Test2::StartServiceRecord> services, const Test2::ServiceLaunchPriority currentPriority);

    /// @brief Asynchronously shuts down services with the specified priority.
    ///
    /// This method marshals the shutdown request to the service host's executor.
    /// The operation will fail gracefully if the service host has been destroyed.
    ///
    /// @param priority The launch priority of services to shut down.
    /// @return A boost::future yielding a vector of exception_ptr for services that failed to shut down.
    ///         Empty vector indicates all services shut down cleanly.
    ///         Use .then() for non-blocking continuation or .get() to block.
    /// @note This method is safe to call from any thread. The actual shutdown logic
    ///       executes on the service host's executor.
    boost::future<std::vector<std::exception_ptr>> TryShutdownServicesAsync(const Test2::ServiceLaunchPriority priority);

    /// @brief Asynchronously requests shutdown of the service host.
    ///
    /// This method marshals the shutdown request to the service host's executor.
    /// The operation will fail gracefully if the service host has been destroyed.
    ///
    /// @return A boost::future yielding true if the shutdown request was successfully
    ///         processed by the service host, or false if the service host is no longer
    ///         available.
    ///         Use .then() for non-blocking continuation or .get() to block.
    /// @note This method is safe to call from any thread. The actual shutdown logic
    ///       executes on the service host's executor.
    boost::future<bool> TryRequestShutdownAsync();

    /// @brief Synchronously posts a shutdown request to the service host's thread.
    ///
    /// This is a fire-and-forget operation that posts the shutdown request without
    /// waiting for completion.
    ///
    /// @return true if the shutdown request was posted successfully (weak_ptr was valid),
    ///         false if the service host has already been destroyed.
    /// @note This is safe to call from any thread, including destructors.
    /// @note This method is noexcept and will not throw exceptions.
    bool TryRequestShutdown() noexcept;
  };
}

#endif
