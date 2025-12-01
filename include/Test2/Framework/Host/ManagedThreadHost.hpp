#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADHOST_HPP
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

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <thread>
#include "ManagedThreadRecord.hpp"
#include "ManagedThreadServiceHost.hpp"

namespace Test2
{
  /// @brief Manages a thread that runs a ManagedThreadServiceHost.
  class ManagedThreadHost
  {
    std::unique_ptr<ManagedThreadServiceHost> m_serviceHost;
    std::thread m_thread;

  public:
    ManagedThreadHost()
      : m_serviceHost(std::make_unique<ManagedThreadServiceHost>())
    {
    }

    ~ManagedThreadHost()
    {
      Stop();
    }

    ManagedThreadHost(const ManagedThreadHost&) = delete;
    ManagedThreadHost& operator=(const ManagedThreadHost&) = delete;
    ManagedThreadHost(ManagedThreadHost&&) = delete;
    ManagedThreadHost& operator=(ManagedThreadHost&&) = delete;

    /// @brief Starts the managed thread.
    /// @param cancel_slot Cancellation slot to stop the thread.
    /// @return An awaitable that completes when the thread has started, containing a ManagedThreadRecord with the lifetime awaitable.
    boost::asio::awaitable<ManagedThreadRecord> StartAsync(boost::asio::cancellation_slot cancel_slot = {})
    {
      auto lifetimePromise = std::make_shared<std::promise<void>>();
      auto lifetimeFuture = lifetimePromise->get_future();
      auto startedPromise = std::make_shared<std::promise<void>>();
      auto startedFuture = startedPromise->get_future();

      if (!m_thread.joinable())
      {
        m_thread = std::thread(
          [this, lifetimePromise, startedPromise, cancel_slot]()
          {
            try
            {
              // Signal that thread has started
              startedPromise->set_value();

              // Run the io_context with cancellation support
              // Note: We can't directly use co_await here since we're in a std::thread
              m_serviceHost->GetIoContext().run();

              // Signal lifetime completion
              lifetimePromise->set_value();
            }
            catch (...)
            {
              lifetimePromise->set_exception(std::current_exception());
            }
          });

        // If cancellation is requested, post a stop to the io_context
        if (cancel_slot.is_connected())
        {
          cancel_slot.assign([this](boost::asio::cancellation_type) { m_serviceHost->GetIoContext().stop(); });
        }
      }

      // Wait for thread to start
      startedFuture.wait();

      // Create the lifetime awaitable from the future
      auto& executor = co_await boost::asio::this_coro::executor;
      co_return ManagedThreadRecord{[](std::shared_future<void> future, auto executor) -> boost::asio::awaitable<void>
                                    {
                                      co_await boost::asio::post(executor, boost::asio::use_awaitable);
                                      future.wait();
                                      co_return;
                                    }(std::move(lifetimeFuture).share(), executor)};
    }

    void Stop()
    {
      if (m_serviceHost)
      {
        // Post the stop call to the managed thread
        boost::asio::post(m_serviceHost->GetIoContext(), [this]() { m_serviceHost->Stop(); });
      }
      if (m_thread.joinable())
      {
        m_thread.join();
      }
    }

    boost::asio::io_context& GetIoContext()
    {
      return m_serviceHost->GetIoContext();
    }

    const boost::asio::io_context& GetIoContext() const
    {
      return m_serviceHost->GetIoContext();
    }
  };
}

#endif