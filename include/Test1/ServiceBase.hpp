#ifndef SERVICE_FRAMEWORK_TEST1_SERVICEBASE_HPP
#define SERVICE_FRAMEWORK_TEST1_SERVICEBASE_HPP
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

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>
#include <atomic>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace Test1
{
  // Base service class - runs in its own thread with coroutine support
  class ServiceBase
  {
    std::string m_serviceName;
    boost::asio::io_context m_io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work_guard;
    std::thread m_thread;

  public:
    explicit ServiceBase(std::string serviceName)
      : m_serviceName(std::move(serviceName))
      , m_work_guard(boost::asio::make_work_guard(m_io_context))
    {
    }

    virtual ~ServiceBase()
    {
      stop();
    }

    void start()
    {
      if (!m_thread.joinable())
      {
        m_thread = std::thread(
          [this]()
          {
            spdlog::info("[{}] Thread started", m_serviceName);
            m_io_context.run();
            spdlog::info("[{}] Thread shutting down", m_serviceName);
          });
      }
    }

    void stop()
    {
      m_work_guard.reset();
      m_io_context.stop();
      if (m_thread.joinable())
      {
        m_thread.join();
      }
    }

  protected:
    boost::asio::io_context& get_io_context()
    {
      return m_io_context;
    }

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
