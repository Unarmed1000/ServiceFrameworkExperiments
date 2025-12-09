#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVE_COOPERATIVETHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVE_COOPERATIVETHREADSERVICEHOST_HPP
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
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Test2
{
  /// @brief Callback type for waking the host thread.
  ///
  /// @note The callback MUST be thread-safe as it may be invoked from any thread
  ///       when asynchronous work is posted to the io_context.
  using WakeCallback = std::function<void()>;

  /// @brief Service host designed for cooperative execution on a UI or main thread.
  ///
  /// Unlike ManagedThreadServiceHost which owns a dedicated thread with a blocking run() loop,
  /// CooperativeThreadServiceHost is designed to integrate with an existing event loop (e.g., UI message pump).
  ///
  /// Key characteristics:
  /// - No work guard - io_context lifetime is managed externally
  /// - poll()-based execution - processes ready handlers without blocking
  /// - Wake callback - notifies the host thread when async work is posted
  /// - Update() convenience method - combines Poll() and ProcessServices()
  ///
  /// Usage pattern:
  /// 1. Create the host
  /// 2. Register a wake callback that signals the main loop to call Update()
  /// 3. Call Update() from the main loop when signaled or on each iteration
  /// 4. Update() returns ProcessResult with sleep hints for the main loop
  class CooperativeThreadServiceHost : public ServiceHostBase
  {
    WakeCallback m_wakeCallback;
    mutable std::mutex m_wakeMutex;


  public:
    /// @brief Construct a CooperativeThreadServiceHost for the current thread.
    ///
    /// The host is bound to the thread that creates it. All thread-sensitive operations
    /// (Poll, Update, SetWakeCallback) must be called from this thread.
    CooperativeThreadServiceHost()
      : ServiceHostBase()
    {
      spdlog::info("CooperativeThreadServiceHost created at {}", static_cast<void*>(this));
    }

    ~CooperativeThreadServiceHost() override
    {
      // Verify shutdown assumptions - log warnings for any violations
      {
        const auto serviceCount = m_provider->GetServiceCount();
        if (serviceCount > 0)
        {
          spdlog::warn("CooperativeThreadServiceHost destroyed with {} services still registered", serviceCount);
        }
      }
      if (!GetIoContext().stopped())
      {
        spdlog::warn("CooperativeThreadServiceHost destroyed while io_context has not been stopped");
      }
      {
        std::lock_guard<std::mutex> lock(m_wakeMutex);
        if (m_wakeCallback)
        {
          spdlog::warn("CooperativeThreadServiceHost destroyed with wake callback still set");
        }
      }
      spdlog::info("CooperativeThreadServiceHost destroying at {}", static_cast<void*>(this));
    }

    /// @brief Set the wake callback to notify the host thread when async work is posted.
    ///
    /// The wake callback is invoked whenever asynchronous work is posted to the io_context,
    /// allowing the host thread's event loop to wake up and call Poll() or Update().
    ///
    /// @param callback The callback to invoke. MUST be thread-safe as it may be called from any thread.
    ///                 Pass nullptr to clear the callback.
    /// @throws WrongThreadException if called from a thread other than the owner thread.
    void SetWakeCallback(WakeCallback callback)
    {
      ValidateThreadAccess();
      std::lock_guard<std::mutex> lock(m_wakeMutex);
      m_wakeCallback = std::move(callback);
    }

    /// @brief Process all ready handlers without blocking.
    ///
    /// Runs the io_context's poll() to process all handlers that are ready to run,
    /// then returns immediately without waiting for new work.
    ///
    /// @return The number of handlers that were executed.
    /// @throws WrongThreadException if called from a thread other than the owner thread.
    std::size_t Poll()
    {
      ValidateThreadAccess();
      return GetIoContext().poll();
    }

    /// @brief Convenience method that polls the io_context and processes all services.
    ///
    /// This is the primary method to call from your main loop. It:
    /// 1. Calls Poll() to process any ready async handlers
    /// 2. Calls ProcessServices() to run service Process() methods
    /// 3. Returns the aggregated ProcessResult with sleep hints
    ///
    /// @return Aggregated ProcessResult from all services.
    /// @throws WrongThreadException if called from a thread other than the owner thread.
    ProcessResult Update()
    {
      ValidateThreadAccess();
      Poll();
      return ProcessServices();
    }

    /// @brief Post work to the io_context and trigger the wake callback.
    ///
    /// Use this method instead of directly posting to the io_context when you want
    /// to ensure the host thread is woken up to process the posted work.
    ///
    /// @tparam Handler The handler type.
    /// @param handler The handler to post.
    template <typename Handler>
    void PostWithWake(Handler&& handler)
    {
      boost::asio::post(GetExecutor(), std::forward<Handler>(handler));
      TriggerWake();
    }

    ProcessResult ProcessServices()
    {
      ValidateThreadAccess();
      return DoProcessServices();
    }

    /// @brief Expose executor access for external coordination.
    using ServiceHostBase::GetExecutor;

    /// @brief Request the io_context to stop.
    ///
    /// This can be called from any thread to stop the io_context.
    void RequestStop()
    {
      GetIoContext().stop();
    }

  private:
    /// @brief Invoke the wake callback if set.
    void TriggerWake()
    {
      WakeCallback callback;
      {
        std::lock_guard<std::mutex> lock(m_wakeMutex);
        callback = m_wakeCallback;
      }

      if (callback)
      {
        callback();
      }
    }
  };
}

#endif
