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
#include <future>
#include <stdexcept>
#include "ManagedThreadServiceHost.hpp"

namespace Test2
{
  ManagedThreadHost::ManagedThreadHost()
  {
  }

  ManagedThreadHost::~ManagedThreadHost()
  {
    // Signal cancellation to stop the io_context
    m_cancellationSignal.emit(boost::asio::cancellation_type::terminal);

    if (m_thread.joinable())
    {
      m_thread.join();
    }
  }

  boost::asio::awaitable<ManagedThreadRecord> ManagedThreadHost::StartAsync(boost::asio::cancellation_slot cancel_slot)
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

    // Create parent cancellation signal that will be shared across thread boundary
    auto parentCancellationSignal = std::make_shared<boost::asio::cancellation_signal>();

    m_thread = std::thread(
      [this, lifetimePromise, startedPromise, parentCancellationSignal]()
      {
        try
        {
          // Construct the service host ON THIS THREAD with parent cancellation slot
          auto serviceHost = std::make_shared<ManagedThreadServiceHost>(parentCancellationSignal->slot());
          m_serviceHostProxy = std::make_shared<ServiceHostProxy>(serviceHost, serviceHost->GetIoContext().get_executor());

          // Signal that thread has started
          startedPromise->set_value();

          // Run the io_context - it will be stopped via the cancellation slot
          serviceHost->GetIoContext().run();

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

    // Register internal cancellation signal to emit parent signal
    m_cancellationSignal.slot().assign([parentCancellationSignal](boost::asio::cancellation_type type) { parentCancellationSignal->emit(type); });

    // Register external cancellation slot to emit parent signal if provided
    if (cancel_slot.is_connected())
    {
      cancel_slot.assign([parentCancellationSignal](boost::asio::cancellation_type type) { parentCancellationSignal->emit(type); });
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

  std::shared_ptr<IThreadSafeServiceHost> ManagedThreadHost::GetServiceHost()
  {
    if (m_serviceHostProxy)
    {
      return m_serviceHostProxy;
    }
    throw std::runtime_error("Service host is no longer available");
  }
}
