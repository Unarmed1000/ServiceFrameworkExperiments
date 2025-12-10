#ifndef SERVICE_FRAMEWORK_TEST3_NOLAMBDA_TOFUTUREWITHCALLBACK_HPP
#define SERVICE_FRAMEWORK_TEST3_NOLAMBDA_TOFUTUREWITHCALLBACK_HPP
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
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <future>
#include <memory>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace Test3
{
  namespace NoLambda
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

    /// @brief Invokes an awaitable-returning function directly and invokes a callback method on completion.
    ///
    /// This version eliminates the lambda wrapper boilerplate - you can call async functions
    /// that return boost::asio::awaitable<T> directly by passing the function and its arguments.
    ///
    /// The callback is invoked on the specified executor when the coroutine completes.
    /// Automatically checks stop_token (if available via GetStopToken()) to safely
    /// skip callback if the object is being destroyed.
    ///
    /// @tparam TCallback Type of the callback receiver.
    /// @tparam CallbackMethod Type of the callback method pointer.
    /// @tparam AwaitableFunc Type of the function returning awaitable<T>.
    /// @tparam Args Types of arguments to pass to the awaitable function.
    /// @param callbackExecutor Executor to run the callback on.
    /// @param callbackObj Pointer to the callback receiver object.
    /// @param callbackMethod Pointer to the callback method (e.g., &ExampleServiceUse::HandleResult).
    /// @param awaitableFunc Function that returns boost::asio::awaitable<T> when invoked with args.
    /// @param args Arguments to forward to the awaitable function.
    /// @return std::future<T> that can be used to wait for or retrieve the result.
    ///
    /// @example
    ///   // Instead of:
    ///   CallAsync(&Handler, [service]() { return service->AddAsync(1.0, 2.0); });
    ///
    ///   // Write:
    ///   CallAsync(&Handler, &IAddService::AddAsync, service, 1.0, 2.0);
    template <typename TCallback, typename CallbackMethod, typename AwaitableFunc, typename... Args>
    auto ToFutureWithCallback(boost::asio::any_io_executor callbackExecutor, TCallback* callbackObj, CallbackMethod callbackMethod,
                              AwaitableFunc awaitableFunc, Args&&... args)
    {
      // Validate that the function is callable with the provided arguments
      static_assert(std::is_invocable_v<AwaitableFunc, Args...>,
                    "Function must be callable with provided arguments. "
                    "Check that argument types match the function signature.");

      // Invoke the function to get the awaitable type and validate it
      using AwaitableType = std::invoke_result_t<AwaitableFunc, Args...>;
      static_assert(Detail::is_awaitable_v<AwaitableType>,
                    "Function must return boost::asio::awaitable<T>. "
                    "If calling a member function, use: &Class::method, objectPtr, args...");

      using ResultType = Detail::awaitable_value_t<AwaitableType>;

      auto promise = std::make_shared<std::promise<ResultType>>();
      auto future = promise->get_future();

      // Get stop token if available (SFINAE-friendly)
      std::stop_token stopToken;
      if constexpr (requires { callbackObj->GetStopToken(); })
      {
        stopToken = callbackObj->GetStopToken();
      }

      boost::asio::co_spawn(
        callbackExecutor,
        [promise, callbackExecutor, callbackObj, callbackMethod, stopToken, awaitableFunc = std::move(awaitableFunc),
         ... capturedArgs = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<void>
        {
          try
          {
            // Invoke the function to get the awaitable, then co_await it
            if constexpr (std::is_void_v<ResultType>)
            {
              co_await std::invoke(std::move(awaitableFunc), std::move(capturedArgs)...);
              promise->set_value();
            }
            else
            {
              auto result = co_await std::invoke(std::move(awaitableFunc), std::move(capturedArgs)...);
              promise->set_value(std::move(result));
            }
          }
          catch (...)
          {
            promise->set_exception(std::current_exception());
          }

          // Post callback to receiver's executor
          boost::asio::post(callbackExecutor,
                            [promise, callbackObj, callbackMethod, stopToken]()
                            {
                              // Check stop token if available
                              if constexpr (requires { stopToken.stop_requested(); })
                              {
                                if (stopToken.stop_requested())
                                {
                                  return;    // Skip callback - object is being destroyed
                                }
                              }

                              // Invoke callback method with future
                              (callbackObj->*callbackMethod)(promise->get_future());
                            });
        },
        boost::asio::detached);

      return future;
    }
  }    // namespace NoLambda
}    // namespace Test3

#endif
