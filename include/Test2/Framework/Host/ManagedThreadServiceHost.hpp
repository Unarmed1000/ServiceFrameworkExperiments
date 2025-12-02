#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
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

#include <Test2/Framework/Host/ServiceHostBase.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

namespace Test2
{
  /// @brief Service host that lives on a managed thread. All methods are called on the managed thread.
  ///
  /// This host owns an io_context with a work guard, keeping the event loop running until
  /// explicitly stopped. Use RunAsync() to start the event loop on the managed thread.
  class ManagedThreadServiceHost : public ServiceHostBase
  {
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;

  public:
    ManagedThreadServiceHost()
      : ServiceHostBase()
      , m_ioContext(std::make_unique<boost::asio::io_context>())
      , m_work(boost::asio::make_work_guard(*m_ioContext))
    {
      spdlog::info("ManagedThreadServiceHost created at {}", static_cast<void*>(this));
    }

    ~ManagedThreadServiceHost() override
    {
      spdlog::info("ManagedThreadServiceHost destroying at {}", static_cast<void*>(this));
      // Called on the managed thread during shutdown
      m_work.reset();
    }

    /// @brief Starts and runs the io_context event loop. Called on the managed thread.
    /// @param cancel_slot Cancellation slot to stop the io_context run loop.
    /// @return An awaitable that completes when the io_context run loop exits.
    boost::asio::awaitable<void> RunAsync(boost::asio::cancellation_slot cancel_slot = {})
    {
      if (cancel_slot.is_connected())
      {
        cancel_slot.assign(
          [this](boost::asio::cancellation_type)
          {
            m_work.reset();
            m_ioContext->stop();
          });
      }

      m_ioContext->run();
      co_return;
    }

    boost::asio::io_context& GetIoContext() override
    {
      return *m_ioContext;
    }

    const boost::asio::io_context& GetIoContext() const override
    {
      return *m_ioContext;
    }

    /// @brief Try to start services at a given priority level.
    /// @param services Services to start.
    /// @param currentPriority Priority level for this group.
    /// @return Awaitable that completes when services are started.
    boost::asio::awaitable<void> TryStartServicesAsync(std::vector<StartServiceRecord> services, ServiceLaunchPriority currentPriority)
    {
      co_await boost::asio::co_spawn(
        *m_ioContext, [this, services = std::move(services), currentPriority]() mutable -> boost::asio::awaitable<void>
        { co_await DoTryStartServicesAsync(std::move(services), currentPriority); }, boost::asio::use_awaitable);
      co_return;
    }
  };
}

#endif