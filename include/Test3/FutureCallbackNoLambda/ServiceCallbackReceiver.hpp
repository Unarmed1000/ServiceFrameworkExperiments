#ifndef SERVICE_FRAMEWORK_TEST3_NOLAMBDA_SERVICECALLBACKRECEIVER_HPP
#define SERVICE_FRAMEWORK_TEST3_NOLAMBDA_SERVICECALLBACKRECEIVER_HPP
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
#include "ToFutureWithCallback.hpp"

namespace Test3
{
  namespace NoLambda
  {
    /// @brief Base class for objects that receive async callbacks from awaitable-returning functions.
    ///
    /// Provides automatic cancellation via stop_token and a convenient CallAsync
    /// helper method. The destructor requests stop, ensuring pending callbacks
    /// can check if the object is still valid before invoking methods.
    ///
    /// Unlike the lambda version (FutureCallback), this accepts awaitable-returning
    /// functions directly without requiring lambda wrappers, eliminating boilerplate.
    ///
    /// Thread-safe when callbacks are posted to the object's executor.
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

      /// @brief Convenient helper to call awaitable-returning functions directly with automatic callback handling.
      ///
      /// Automatically passes this object's executor and 'this' pointer to ToFutureWithCallback,
      /// simplifying the call site. Eliminates lambda wrapper boilerplate.
      ///
      /// @tparam CallbackMethod Type of the member function pointer for the callback.
      /// @tparam AwaitableFunc Type of the function returning awaitable<T>.
      /// @tparam Args Types of arguments to pass to the awaitable function.
      /// @param method Pointer to the callback method (e.g., &ExampleServiceUse::HandleResult).
      /// @param awaitableFunc Function that returns boost::asio::awaitable<T>.
      /// @param args Arguments to pass to the awaitable function.
      /// @return std::future<T> that can be used to wait for or retrieve the result.
      ///
      /// @example
      ///   // Direct call without lambda wrapper:
      ///   CallAsync(&MyClass::OnResult, &IAddService::AddAsync, addService, 1.0, 2.0);
      template <typename CallbackMethod, typename AwaitableFunc, typename... Args>
      auto CallAsync(CallbackMethod method, AwaitableFunc awaitableFunc, Args&&... args)
      {
        return ToFutureWithCallback(m_executor, this, method, std::move(awaitableFunc), std::forward<Args>(args)...);
      }
    };
  }    // namespace NoLambda
}    // namespace Test3

#endif
