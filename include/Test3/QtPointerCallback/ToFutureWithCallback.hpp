#ifndef SERVICE_FRAMEWORK_TEST3_QTPOINTER_TOFUTUREWITHCALLBACK_HPP
#define SERVICE_FRAMEWORK_TEST3_QTPOINTER_TOFUTUREWITHCALLBACK_HPP
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
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace Test3
{
  namespace QtPointer
  {
    namespace Detail
    {
      // Helper to detect if a type is boost::asio::awaitable<T>
      template <typename>
      struct is_awaitable : std::false_type
      {
      };

      template <typename T>
      struct is_awaitable<boost::asio::awaitable<T>> : std::true_type
      {
        using value_type = T;
      };

      template <typename T>
      inline constexpr bool is_awaitable_v = is_awaitable<T>::value;

      template <typename T>
      using awaitable_value_t = typename is_awaitable<T>::value_type;
    }    // namespace Detail

    /// @brief Wraps a coroutine lambda and invokes a callback method on completion with QPointer safety.
    ///
    /// This implementation uses QPointer to safely check if the callback object still exists
    /// before invoking the callback. This provides automatic protection against use-after-free
    /// without requiring Qt's MOC or slot declarations.
    ///
    /// The callback is invoked on the QObject's thread when the coroutine completes, using
    /// Qt's queued connection mechanism for thread-safe marshaling.
    ///
    /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
    /// @tparam CallbackMethod Type of the callback method pointer.
    /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
    /// @param workExecutor Executor to run the coroutine on.
    /// @param callbackObj Pointer to the QObject callback receiver.
    /// @param callbackMethod Pointer to the callback method (e.g., &MyClass::handleResult).
    /// @param coroutineLambda Lambda that returns boost::asio::awaitable<T>.
    /// @return std::future<T> that can be used to wait for or retrieve the result.
    template <typename TCallback, typename CallbackMethod, typename CoroutineLambda>
    auto ToFutureWithCallback(boost::asio::any_io_executor workExecutor, TCallback* callbackObj, CallbackMethod callbackMethod,
                              CoroutineLambda coroutineLambda)
    {
      using AwaitableType = std::invoke_result_t<CoroutineLambda>;
      static_assert(Detail::is_awaitable_v<AwaitableType>, "Lambda must return boost::asio::awaitable<T>");
      using ResultType = Detail::awaitable_value_t<AwaitableType>;

      auto promise = std::make_shared<std::promise<ResultType>>();
      auto future = promise->get_future();

      // Create QPointer for safe lifetime checking
      QPointer<TCallback> safePtr(callbackObj);

      boost::asio::co_spawn(
        workExecutor,
        [promise, safePtr, callbackMethod, coroutineLambda = std::move(coroutineLambda)]() mutable -> boost::asio::awaitable<void>
        {
          try
          {
            if constexpr (std::is_void_v<ResultType>)
            {
              co_await coroutineLambda();
              promise->set_value();
            }
            else
            {
              auto result = co_await coroutineLambda();
              promise->set_value(result);
            }
          }
          catch (...)
          {
            promise->set_exception(std::current_exception());
          }

          // Check if object still exists before invoking callback
          if (!safePtr.isNull())
          {
            // Invoke callback on QObject's thread using Qt's queued connection
            QMetaObject::invokeMethod(
              safePtr.data(),
              [promise, safePtr, callbackMethod]()
              {
                // Double-check in case object was destroyed between queue and execution
                if (!safePtr.isNull())
                {
                  // Invoke callback method with future
                  (safePtr.data()->*callbackMethod)(promise->get_future());
                }
              },
              Qt::QueuedConnection);
          }
        },
        boost::asio::detached);

      return future;
    }
  }    // namespace QtPointer
}    // namespace Test3

#endif
