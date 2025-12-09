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
#include <Test2/Framework/Host/ServiceHostProxy.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <future>
#include <stdexcept>
#include "../ServiceHostBase.hpp"
#include "ManagedThreadServiceHost.hpp"

namespace Test2
{
  ManagedThreadHost::ManagedThreadHost(Lifecycle::ExecutorContext<ILifeTracker> sourceContext)
    : m_sourceContext(std::move(sourceContext))
  {
  }

  ManagedThreadHost::~ManagedThreadHost()
  {
    if (m_thread.joinable())
    {
      spdlog::warn("ManagedThreadHost::~ManagedThreadHost: thread has not been properly shut down, forcing join");

      if (m_serviceHostProxy)
      {
        spdlog::warn("ManagedThreadHost::~ManagedThreadHost: requesting shutdown of service host");
        m_serviceHostProxy->TryRequestShutdown();
      }

      m_thread.join();
    }
  }

  boost::asio::awaitable<ManagedThreadRecord> ManagedThreadHost::StartAsync()
  {
    // Guard against multiple starts
    if (m_thread.joinable())
    {
      throw std::runtime_error("ManagedThreadHost has already been started");
    }

    auto lifetimePromise = std::make_shared<std::promise<void>>();
    auto lifetimeFuture = lifetimePromise->get_future();
    auto startedPromise = std::make_shared<std::promise<void>>();
    auto startedFuture = startedPromise->get_future();

    m_thread = std::thread(
      [this, lifetimePromise, startedPromise]()
      {
        try
        {
          // Construct the service host ON THIS THREAD with parent cancellation slot
          auto serviceHost = std::make_shared<ManagedThreadServiceHost>();
          m_serviceHostProxy = std::make_shared<ServiceHostProxy>(Lifecycle::DispatchContext(m_sourceContext, serviceHost));

          // Signal that thread has started
          startedPromise->set_value();

          // Run the io_context - it will be stopped via the cancellation slot
          serviceHost->Run();

          // Signal lifetime completion
          lifetimePromise->set_value();
        }
        catch (...)
        {
          lifetimePromise->set_exception(std::current_exception());
        }
      });

    // Wait for thread to start and serviceHost to be assigned
    startedFuture.wait();

    if (!m_serviceHostProxy)
    {
      throw std::runtime_error("ManagedThreadHost failed to start service host");
    }

    // Create the lifetime awaitable from the future
    auto executor = co_await boost::asio::this_coro::executor;
    co_return ManagedThreadRecord{[](std::shared_future<void> future, auto exec) -> boost::asio::awaitable<void>
                                  {
                                    co_await boost::asio::post(exec, boost::asio::use_awaitable);
                                    future.wait();
                                    co_return;
                                  }(std::move(lifetimeFuture).share(), executor)};
  }


  boost::asio::awaitable<bool> ManagedThreadHost::TryShutdownAsync()
  {
    // Guard against multiple starts
    if (!m_thread.joinable() || !m_serviceHostProxy)
    {
      co_return false;
    }

    bool result = co_await m_serviceHostProxy->TryRequestShutdownAsync();

    // Wait for the thread to complete after requesting shutdown
    if (m_thread.joinable())
    {
      auto executor = co_await boost::asio::this_coro::executor;
      co_await boost::asio::post(executor, boost::asio::use_awaitable);
      m_thread.join();
    }

    co_return result;
  }


  std::shared_ptr<IThreadSafeServiceHost> ManagedThreadHost::GetServiceHost()
  {
    if (m_serviceHostProxy)
    {
      return m_serviceHostProxy;
    }
    throw std::runtime_error("Service host is no longer available");
  }
}
