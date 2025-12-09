#ifndef TEST_MANAGEDTHREADLIFECYCLE_HPP
#define TEST_MANAGEDTHREADLIFECYCLE_HPP
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

#include <Test2/Framework/Host/Managed/ManagedThreadHost.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/asio/awaitable.hpp>
#include <set>
#include <vector>

namespace Test2
{
  /// @brief Test helper that wraps ManagedThreadHost and tracks service lifecycle for automatic cleanup.
  ///
  /// Similar to LifecycleManager but for a single ManagedThreadHost in tests.
  /// Tracks which service priorities have been started and shut down, enabling:
  /// - Automatic service cleanup in test teardown
  /// - Detection of services that weren't properly shut down
  /// - Detailed error reporting with exception messages
  ///
  /// Usage in tests:
  /// 1. Call StartAsync() to start the managed thread
  /// 2. Call StartServicesAsync(services, priority) to start and track services
  /// 3. Optionally call ShutdownServicesAsync(priority) to explicitly shutdown
  /// 4. Call TryShutdownAsync() to shutdown all remaining services and the thread
  class TestManagedThreadLifecycle
  {
    ManagedThreadHost& m_host;
    std::vector<ServiceLaunchPriority> m_startedPriorities;
    std::set<ServiceLaunchPriority> m_shutdownPriorities;

  public:
    /// @brief Constructs the lifecycle helper wrapping a ManagedThreadHost.
    explicit TestManagedThreadLifecycle(ManagedThreadHost& host);

    ~TestManagedThreadLifecycle() = default;

    TestManagedThreadLifecycle(const TestManagedThreadLifecycle&) = delete;
    TestManagedThreadLifecycle& operator=(const TestManagedThreadLifecycle&) = delete;
    TestManagedThreadLifecycle(TestManagedThreadLifecycle&&) = delete;
    TestManagedThreadLifecycle& operator=(TestManagedThreadLifecycle&&) = delete;

    /// @brief Starts the managed thread.
    boost::asio::awaitable<void> StartAsync();

    /// @brief Starts services at the specified priority and tracks them for cleanup.
    ///
    /// @param services Services to start
    /// @param priority Priority level for the services
    boost::asio::awaitable<void> StartServicesAsync(std::vector<StartServiceRecord> services, ServiceLaunchPriority priority);

    /// @brief Shuts down services at the specified priority and tracks the shutdown.
    ///
    /// @param priority Priority level to shutdown
    /// @return Vector of exceptions that occurred during shutdown
    boost::asio::awaitable<std::vector<std::exception_ptr>> ShutdownServicesAsync(ServiceLaunchPriority priority);

    /// @brief Shuts down all services that haven't been explicitly shut down yet.
    ///
    /// Iterates through started priorities in reverse order, skipping those already shut down.
    /// Collects all exceptions and returns them for error reporting.
    ///
    /// @return Vector of exceptions that occurred during shutdown
    boost::asio::awaitable<std::vector<std::exception_ptr>> ShutdownAllServicesAsync();

    /// @brief Shuts down all services and then the managed thread.
    ///
    /// This is the main cleanup method that should be called in test teardown.
    /// Shuts down services in reverse priority order, then stops the thread.
    ///
    /// @return Vector of exceptions that occurred during shutdown
    boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownAsync();

    /// @brief Gets the underlying ManagedThreadHost for direct access when needed.
    ManagedThreadHost& GetHost() noexcept;

    /// @brief Gets the underlying ManagedThreadHost for direct access when needed (const).
    const ManagedThreadHost& GetHost() const noexcept;
  };
}

#endif
