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

#include <Common/SpdLogHelper.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>

namespace Test2
{
  /// @brief Service host that lives on a managed thread. All methods are called on the managed thread.
  class ManagedThreadServiceHost
  {
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;

    static std::shared_ptr<spdlog::logger> GetLogger()
    {
      LOGGER_NAME(ManagedThreadServiceHost);
      static const auto logger = SpdLogHelper::GetLogger<LoggerName_ManagedThreadServiceHost>();
      return logger;
    }

  public:
    ManagedThreadServiceHost()
      : m_ioContext(std::make_unique<boost::asio::io_context>())
      , m_work(boost::asio::make_work_guard(*m_ioContext))
    {
      GetLogger()->trace("Created at {}", static_cast<void*>(this));
    }

    ~ManagedThreadServiceHost()
    {
      GetLogger()->trace("Destroying at {}", static_cast<void*>(this));
      // Called on the managed thread during shutdown
      m_work.reset();
    }

    ManagedThreadServiceHost(const ManagedThreadServiceHost&) = delete;
    ManagedThreadServiceHost& operator=(const ManagedThreadServiceHost&) = delete;
    ManagedThreadServiceHost(ManagedThreadServiceHost&&) = delete;
    ManagedThreadServiceHost& operator=(ManagedThreadServiceHost&&) = delete;

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

    boost::asio::io_context& GetIoContext()
    {
      return *m_ioContext;
    }

    const boost::asio::io_context& GetIoContext() const
    {
      return *m_ioContext;
    }

  protected:
    // Execute function on service thread - handles both copyable and move-only types
    template <typename Func>
    auto call(Func&& func) -> boost::asio::awaitable<decltype(std::declval<std::decay_t<Func>>()())>
    {
      using ResultType = decltype(std::declval<std::decay_t<Func>>()());

      // Use co_spawn to execute on service thread
      co_return co_await boost::asio::co_spawn(
        m_io_context,
        [func = std::forward<Func>(func)]() mutable -> boost::asio::awaitable<ResultType>
        {
          if constexpr (std::is_void_v<ResultType>)
          {
            func();
            co_return;
          }
          else
          {
            co_return func();
          }
        },
        boost::asio::use_awaitable);
    }
  };
}

#endif