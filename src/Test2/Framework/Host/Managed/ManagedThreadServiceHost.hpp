#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADSERVICEHOST_HPP
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
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;

  public:
    /// @brief Constructs a ManagedThreadServiceHost.
    /// @param cancel_slot Optional cancellation slot to stop the io_context.
    explicit ManagedThreadServiceHost(boost::asio::cancellation_slot cancel_slot = {})
      : ServiceHostBase()
      , m_work(boost::asio::make_work_guard(m_ioContext))
    {
      spdlog::info("ManagedThreadServiceHost created at {}", static_cast<void*>(this));

      // Register cancellation handler to stop the io_context
      if (cancel_slot.is_connected())
      {
        cancel_slot.assign([this](boost::asio::cancellation_type) { m_ioContext.stop(); });
      }
    }

    ~ManagedThreadServiceHost() override
    {
      spdlog::info("ManagedThreadServiceHost destroying at {}", static_cast<void*>(this));
      // Called on the managed thread during shutdown
      m_work.reset();
    }

    void RequestShutdown() final
    {
      ServiceHostBase::RequestShutdown();
      m_work.reset();
    }

    void Run()
    {
      DoRun();
    }
  };
}

#endif