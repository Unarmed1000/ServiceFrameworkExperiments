#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVETHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVETHREADSERVICEHOST_HPP
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
#include <spdlog/spdlog.h>
#include <functional>
#include <memory>
#include <mutex>
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
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    WakeCallback m_wakeCallback;
    mutable std::mutex m_wakeMutex;

  public:
    CooperativeThreadServiceHost()
      : ServiceHostBase()
      , m_ioContext(std::make_unique<boost::asio::io_context>())
    {
      spdlog::info("CooperativeThreadServiceHost created at {}", static_cast<void*>(this));
    }

    ~CooperativeThreadServiceHost() override
    {
      spdlog::info("CooperativeThreadServiceHost destroying at {}", static_cast<void*>(this));
    }

    /// @brief Set the wake callback to notify the host thread when async work is posted.
    ///
    /// The wake callback is invoked whenever asynchronous work is posted to the io_context,
    /// allowing the host thread's event loop to wake up and call Poll() or Update().
    ///
    /// @param callback The callback to invoke. MUST be thread-safe as it may be called from any thread.
    ///                 Pass nullptr to clear the callback.
    void SetWakeCallback(WakeCallback callback)
    {
      std::lock_guard<std::mutex> lock(m_wakeMutex);
      m_wakeCallback = std::move(callback);
    }

    boost::asio::io_context& GetIoContext() override
    {
      return *m_ioContext;
    }

    const boost::asio::io_context& GetIoContext() const override
    {
      return *m_ioContext;
    }

    /// @brief Process all ready handlers without blocking.
    ///
    /// Runs the io_context's poll() to process all handlers that are ready to run,
    /// then returns immediately without waiting for new work.
    ///
    /// @return The number of handlers that were executed.
    std::size_t Poll()
    {
      return m_ioContext->poll();
    }

    /// @brief Convenience method that polls the io_context and processes all services.
    ///
    /// This is the primary method to call from your main loop. It:
    /// 1. Calls Poll() to process any ready async handlers
    /// 2. Calls ProcessServices() to run service Process() methods
    /// 3. Returns the aggregated ProcessResult with sleep hints
    ///
    /// @return Aggregated ProcessResult from all services.
    ProcessResult Update()
    {
      Poll();
      return ProcessServices();
    }

    /// @brief Try to start services at a given priority level.
    ///
    /// Services are created, initialized, and registered with the provider.
    /// On failure, successfully initialized services are rolled back.
    ///
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
      boost::asio::post(*m_ioContext, std::forward<Handler>(handler));
      TriggerWake();
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
