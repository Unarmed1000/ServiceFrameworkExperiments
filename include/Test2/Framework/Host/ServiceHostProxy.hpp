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
    ///! Weak pointer to the lifetime of the service host.
    std::weak_ptr<ServiceHostBase> m_service;
    boost::asio::any_io_executor m_executor;

  public:
    /// @brief Constructs a proxy that marshals operations to the given executor.
    /// @param service Weak pointer to the service host.
    /// @param executor The executor of the target service host's io_context.
    explicit ServiceHostProxy(std::shared_ptr<ServiceHostBase> service);
    ~ServiceHostProxy();

    //! @see IThreadSafeServiceHost
    boost::asio::awaitable<void> TryStartServicesAsync(std::vector<StartServiceRecord> services, const ServiceLaunchPriority currentPriority) final;
    //! @see IThreadSafeServiceHost
    boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownServicesAsync(const ServiceLaunchPriority priority) final;

    //! @brief Attempts to request shutdown of the service host.
    boost::asio::awaitable<bool> TryRequestShutdownAsync();
  };
}

#endif
