#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEHOSTPROXY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEHOSTPROXY_HPP
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

#include <Test2/Framework/Host/IThreadSafeServiceHost.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <exception>
#include <memory>
#include <stdexcept>
#include <vector>

namespace Test2
{
  class ServiceHostBase;

  /// @brief Thread-safe proxy for a ServiceHostBase running on another thread.
  ///
  /// This proxy can be safely used from any thread to invoke operations on a
  /// service host that lives on a different thread. All operations are marshalled
  /// to the target executor using co_spawn.
  class ServiceHostProxy final : public IThreadSafeServiceHost
  {
    ///! Executor context for the service host (contains weak_ptr and executor).
    Lifecycle::ExecutorContext<ServiceHostBase> m_context;

  public:
    /// @brief Constructs a proxy that marshals operations to the given service host.
    /// @param context Executor context containing the service host and its executor.
    explicit ServiceHostProxy(Lifecycle::ExecutorContext<ServiceHostBase> context);
    ~ServiceHostProxy();

    //! @see IThreadSafeServiceHost
    boost::asio::awaitable<void> TryStartServicesAsync(std::vector<StartServiceRecord> services, const ServiceLaunchPriority currentPriority) final;
    //! @see IThreadSafeServiceHost
    boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownServicesAsync(const ServiceLaunchPriority priority) final;

    //! @brief Asynchronously attempts to request shutdown of the service host.
    //!
    //! This method marshals the shutdown request to the service host's executor and
    //! co_awaits the result. The operation will fail gracefully if the service host
    //! has been destroyed (weak_ptr expired).
    //!
    //! @return An awaitable that yields true if the shutdown request was successfully
    //!         processed by the service host, or false if the service host is no longer
    //!         available.
    //! @note This method is safe to call from any thread. The actual shutdown logic
    //!       executes on the service host's executor.
    boost::asio::awaitable<bool> TryRequestShutdownAsync();

    //! @brief Synchronously posts a shutdown request to the service host's thread.
    //!
    //! This is a fire-and-forget operation that posts the shutdown request without
    //! waiting for completion. Unlike TryRequestShutdownAsync, this method does not
    //! use coroutines and returns immediately after posting.
    //!
    //! @return true if the shutdown request was posted successfully (weak_ptr was valid),
    //!         false if the service host has already been destroyed.
    //! @note This is safe to call from any thread, including destructors.
    //! @note This method is noexcept and will not throw exceptions.
    bool TryRequestShutdown() noexcept;
  };
}

#endif
