#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_STOPTOKEN_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_STOPTOKEN_HPP
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
#include <boost/asio/post.hpp>
#include <boost/thread/future.hpp>
#include <stop_token>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Attaches a callback to a boost::future with std::stop_token lifetime tracking and executor marshaling.
  ///
  /// The callback will be marshaled to the specified executor and will only execute
  /// if the stop_token has not been requested (object still alive). This provides
  /// thread-safe lifetime tracking using std::stop_token.
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::Create(future, executor, this, &MyClass::OnComplete, stopToken);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TCallback Type of the callback receiver.
  /// @tparam CallbackMethod Type of the member function pointer.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor to invoke the callback on.
  /// @param callbackObj Pointer to the callback receiver object.
  /// @param callbackMethod Pointer to the member function to invoke.
  /// @param stopToken Stop token for lifetime tracking.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename CallbackMethod>
  auto Create(boost::future<TResult> future, boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod,
              std::stop_token stopToken)
  {
    return future.then(
      [executor = std::move(executor), callbackObj, callbackMethod, stopToken = std::move(stopToken)](boost::future<TResult> f) mutable
      {
        // Marshal callback to the specified executor
        boost::asio::post(executor,
                          [callbackObj, callbackMethod, stopToken = std::move(stopToken), f = std::move(f)]() mutable
                          {
                            // Check if object is still alive
                            if (stopToken.stop_requested())
                            {
                              return;    // Object destroyed, skip callback
                            }

                            // Invoke the callback method
                            (callbackObj->*callbackMethod)(std::move(f));
                          });
      });
  }

  /// @brief Attaches a callback to a boost::future with automatic stop_token detection and executor marshaling.
  ///
  /// The callback will be marshaled to the specified executor. If the callback object has a
  /// GetStopToken() method (e.g., inherits from ServiceCallbackReceiver), it will be automatically
  /// used for lifetime tracking. Otherwise, an empty stop_token is used (callback always invoked).
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::Create(future, executor, this, &MyClass::OnComplete);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TCallback Type of the callback receiver.
  /// @tparam CallbackMethod Type of the member function pointer.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor to invoke the callback on.
  /// @param callbackObj Pointer to the callback receiver object.
  /// @param callbackMethod Pointer to the member function to invoke.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename CallbackMethod>
  auto Create(boost::future<TResult> future, boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
  {
    std::stop_token stopToken;

    // Automatically get stop token if available (SFINAE-friendly)
    if constexpr (requires { callbackObj->GetStopToken(); })
    {
      stopToken = callbackObj->GetStopToken();
    }

    return Create(std::move(future), std::move(executor), callbackObj, callbackMethod, std::move(stopToken));
  }

  /// @brief Attaches a lambda callback to a boost::future with std::stop_token lifetime tracking and executor marshaling.
  ///
  /// The callback will be marshaled to the specified executor and will only execute
  /// if the stop_token has not been requested (object still alive). This provides
  /// thread-safe lifetime tracking using std::stop_token with lambda support.
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::Create(future, executor, [this](boost::future<void> result) {
  ///     try {
  ///         result.get();
  ///         // Success, can capture state and use inline logic
  ///     } catch (...) {
  ///         // Error handling
  ///     }
  /// }, stopToken);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TLambda Type of the lambda/callable.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor to invoke the callback on.
  /// @param lambda The lambda or callable to invoke with the future result.
  /// @param stopToken Stop token for lifetime tracking.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TLambda>
  auto Create(boost::future<TResult> future, boost::asio::any_io_executor executor, TLambda&& lambda, std::stop_token stopToken)
  {
    return future.then(
      [executor = std::move(executor), lambda = std::forward<TLambda>(lambda), stopToken = std::move(stopToken)](boost::future<TResult> f) mutable
      {
        // Marshal callback to the specified executor
        boost::asio::post(executor,
                          [lambda = std::move(lambda), stopToken = std::move(stopToken), f = std::move(f)]() mutable
                          {
                            // Check if object is still alive
                            if (stopToken.stop_requested())
                            {
                              return;    // Object destroyed, skip callback
                            }

                            // Invoke the lambda
                            lambda(std::move(f));
                          });
      });
  }

  /// @brief Attaches a lambda callback to a boost::future with executor marshaling (no lifetime tracking).
  ///
  /// The callback will be marshaled to the specified executor. Use this when you don't need
  /// lifetime tracking or when the lambda captures appropriate lifetime management.
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::Create(future, executor, [](boost::future<void> result) {
  ///     try {
  ///         result.get();
  ///         // Success
  ///     } catch (...) {
  ///         // Error handling
  ///     }
  /// });
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TLambda Type of the lambda/callable.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor to invoke the callback on.
  /// @param lambda The lambda or callable to invoke with the future result.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TLambda>
  auto Create(boost::future<TResult> future, boost::asio::any_io_executor executor, TLambda&& lambda)
  {
    return future.then(
      [executor = std::move(executor), lambda = std::forward<TLambda>(lambda)](boost::future<TResult> f) mutable
      {
        // Marshal callback to the specified executor
        boost::asio::post(executor,
                          [lambda = std::move(lambda), f = std::move(f)]() mutable
                          {
                            // Invoke the lambda
                            lambda(std::move(f));
                          });
      });
  }
}

#endif
