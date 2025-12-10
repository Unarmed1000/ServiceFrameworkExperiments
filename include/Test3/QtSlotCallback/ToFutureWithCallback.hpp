#ifndef SERVICE_FRAMEWORK_TEST3_QTSLOT_TOFUTUREWITHCALLBACK_HPP
#define SERVICE_FRAMEWORK_TEST3_QTSLOT_TOFUTUREWITHCALLBACK_HPP
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
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace Test3
{
  namespace QtSlot
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

    /// @brief Result structure containing both future and Qt connection for advanced usage.
    ///
    /// This allows explicit control over the connection lifetime, enabling manual
    /// disconnection before the async operation completes if needed.
    ///
    /// @tparam T The result type of the async operation.
    template <typename T>
    struct QtAsyncResult
    {
      std::future<T> future;                 ///< Future containing the result or exception.
      QMetaObject::Connection connection;    ///< Qt connection handle for the callback.
    };

    /// @brief Wraps a coroutine lambda and invokes a Qt slot callback on completion (simple version).
    ///
    /// This version automatically manages the Qt connection and returns only the future.
    /// The callback slot is invoked on the QObject's thread when the coroutine completes.
    /// The slot must be declared with the Q_OBJECT macro and slots keyword.
    ///
    /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
    /// @tparam CallbackMethod Type of the callback slot method pointer.
    /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
    /// @param workExecutor Executor to run the coroutine on.
    /// @param callbackObj Pointer to the QObject callback receiver.
    /// @param callbackMethod Pointer to the callback slot method (e.g., &MyClass::handleResult).
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

      boost::asio::co_spawn(
        workExecutor,
        [promise, callbackObj, callbackMethod, coroutineLambda = std::move(coroutineLambda)]() mutable -> boost::asio::awaitable<void>
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

          // Invoke callback slot on QObject's thread using Qt's queued connection
          QMetaObject::invokeMethod(
            callbackObj,
            [promise, callbackObj, callbackMethod]()
            {
              // Invoke callback slot with future
              (callbackObj->*callbackMethod)(promise->get_future());
            },
            Qt::QueuedConnection);
        },
        boost::asio::detached);

      return future;
    }

    /// @brief Wraps a coroutine lambda and invokes a Qt slot callback on completion (advanced version).
    ///
    /// This version returns both the future and the Qt connection handle, allowing explicit
    /// control over the connection lifetime. You can disconnect the callback before the
    /// async operation completes if needed (e.g., for cancellation scenarios).
    ///
    /// The callback slot is invoked on the QObject's thread when the coroutine completes.
    /// The slot must be declared with the Q_OBJECT macro and slots keyword.
    ///
    /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
    /// @tparam CallbackMethod Type of the callback slot method pointer.
    /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
    /// @param workExecutor Executor to run the coroutine on.
    /// @param callbackObj Pointer to the QObject callback receiver.
    /// @param callbackMethod Pointer to the callback slot method (e.g., &MyClass::handleResult).
    /// @param coroutineLambda Lambda that returns boost::asio::awaitable<T>.
    /// @return QtAsyncResult<T> containing both future and connection handle.
    template <typename TCallback, typename CallbackMethod, typename CoroutineLambda>
    auto ToFutureWithCallbackTracked(boost::asio::any_io_executor workExecutor, TCallback* callbackObj, CallbackMethod callbackMethod,
                                     CoroutineLambda coroutineLambda)
    {
      using AwaitableType = std::invoke_result_t<CoroutineLambda>;
      static_assert(Detail::is_awaitable_v<AwaitableType>, "Lambda must return boost::asio::awaitable<T>");
      using ResultType = Detail::awaitable_value_t<AwaitableType>;

      auto promise = std::make_shared<std::promise<ResultType>>();
      auto future = promise->get_future();

      // Store connection handle for return
      auto connectionPtr = std::make_shared<QMetaObject::Connection>();

      boost::asio::co_spawn(
        workExecutor,
        [promise, callbackObj, callbackMethod, connectionPtr, coroutineLambda = std::move(coroutineLambda)]() mutable -> boost::asio::awaitable<void>
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

          // Invoke callback slot on QObject's thread using Qt's queued connection
          // Check if connection was disconnected before invoking
          if (*connectionPtr)
          {
            QMetaObject::invokeMethod(
              callbackObj,
              [promise, callbackObj, callbackMethod]()
              {
                // Invoke callback slot with future
                (callbackObj->*callbackMethod)(promise->get_future());
              },
              Qt::QueuedConnection);
          }
        },
        boost::asio::detached);

      // Create a dummy connection that we can disconnect
      // Note: This is a simplified approach. In production, you might want to track this more robustly.
      *connectionPtr = QObject::connect(callbackObj, &QObject::destroyed, [connectionPtr]() { *connectionPtr = QMetaObject::Connection(); });

      return QtAsyncResult<ResultType>{std::move(future), *connectionPtr};
    }
  }    // namespace QtSlot
}    // namespace Test3

#endif
