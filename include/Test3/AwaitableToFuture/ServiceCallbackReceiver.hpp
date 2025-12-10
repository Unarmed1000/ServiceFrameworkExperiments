#ifndef SERVICE_FRAMEWORK_TEST3_SERVICECALLBACKRECEIVER_HPP
#define SERVICE_FRAMEWORK_TEST3_SERVICECALLBACKRECEIVER_HPP
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

#include <boost/asio/any_io_executor.hpp>
#include <stop_token>
#include <utility>

namespace Test3
{
  // Forward declare ToFutureWithCallback
  template <typename TCallback, typename CallbackMethod, typename CoroutineLambda>
  auto ToFutureWithCallback(boost::asio::any_io_executor callbackExecutor, TCallback* callbackObj, CallbackMethod callbackMethod,
                            CoroutineLambda coroutineLambda);

  /// @brief Base class for objects that receive async callbacks.
  ///
  /// Provides automatic cancellation via stop_token and a convenient CallAsync
  /// helper method. The destructor requests stop, ensuring pending callbacks
  /// can check if the object is still valid before invoking methods.
  ///
  /// Thread-safe when used with single-threaded executor model where
  /// callbacks are posted to the same io_context as the object.
  class ServiceCallbackReceiver
  {
    std::stop_source m_stopSource;
    boost::asio::any_io_executor m_executor;

  protected:
    /// @brief Constructs a callback receiver with an executor.
    /// @param executor The executor associated with this object's thread.
    explicit ServiceCallbackReceiver(boost::asio::any_io_executor executor)
      : m_executor(std::move(executor))
    {
    }

    ~ServiceCallbackReceiver()
    {
      m_stopSource.request_stop();
    }

    // Non-copyable, non-movable
    ServiceCallbackReceiver(const ServiceCallbackReceiver&) = delete;
    ServiceCallbackReceiver& operator=(const ServiceCallbackReceiver&) = delete;
    ServiceCallbackReceiver(ServiceCallbackReceiver&&) = delete;
    ServiceCallbackReceiver& operator=(ServiceCallbackReceiver&&) = delete;

  public:
    /// @brief Gets the executor for this callback receiver.
    [[nodiscard]] const boost::asio::any_io_executor& GetExecutor() const noexcept
    {
      return m_executor;
    }

    /// @brief Gets the stop token for this callback receiver.
    [[nodiscard]] std::stop_token GetStopToken() const noexcept
    {
      return m_stopSource.get_token();
    }

    /// @brief Checks if stop has been requested (object is being destroyed).
    [[nodiscard]] bool IsStopRequested() const noexcept
    {
      return m_stopSource.stop_requested();
    }

    /// @brief Manually request stop (for early cancellation).
    void RequestStop()
    {
      m_stopSource.request_stop();
    }

    /// @brief Convenient helper to call async operations with automatic callback handling.
    ///
    /// Automatically passes this object's executor and 'this' pointer to ToFutureWithCallback,
    /// simplifying the call site to just the callback method and coroutine lambda.
    ///
    /// @tparam CallbackMethod Type of the member function pointer for the callback.
    /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
    /// @param method Pointer to the callback method (e.g., &ExampleServiceUse::HandleResult).
    /// @param lambda Lambda that returns boost::asio::awaitable<T>.
    /// @return std::future<T> that can be used to wait for or retrieve the result.
    template <typename CallbackMethod, typename CoroutineLambda>
    auto CallAsync(CallbackMethod method, CoroutineLambda lambda)
    {
      return ToFutureWithCallback(m_executor, this, method, std::move(lambda));
    }
  };
}    // namespace Test3

#endif
