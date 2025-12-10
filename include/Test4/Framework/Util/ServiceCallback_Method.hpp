#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_METHOD_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_METHOD_HPP
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

#include <Test4/Framework/Util/CompletionCallback.hpp>
#include <Test4/Framework/Util/ServiceCallback_Method_Internal.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <future>
#include <memory>
#include <stop_token>

namespace Test4
{
  namespace ServiceCallback
  {
    /// @brief Creates a method callback with explicit stop_token lifetime tracking.
    ///
    /// The callback will be marshaled to the specified executor and will only
    /// be invoked if the stop_token has not been requested (object still alive).
    ///
    /// @tparam TResult Type of the future result.
    /// @tparam TCallback Type of the callback receiver.
    /// @tparam CallbackMethod Type of the member function pointer.
    /// @param executor Executor to invoke the callback on.
    /// @param callbackObj Pointer to the callback receiver object.
    /// @param callbackMethod Pointer to the member function to invoke.
    /// @param stopToken Stop token for lifetime tracking.
    /// @return CompletionCallback that can be passed to async methods.
    template <typename TResult, typename TCallback, typename CallbackMethod>
    CompletionCallback<TResult> CreateCallback(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod,
                                               std::stop_token stopToken)
    {
      auto impl = std::make_unique<Internal::MethodCallbackImpl<TResult, TCallback, CallbackMethod>>(std::move(executor), callbackObj, callbackMethod,
                                                                                                     std::move(stopToken));

      return CompletionCallback<TResult>(std::move(impl));
    }

    /// @brief Creates a method callback with automatic stop_token detection.
    ///
    /// If the callback object has a GetStopToken() method (e.g., inherits from
    /// ServiceCallbackReceiver), it will be automatically used for lifetime tracking.
    /// Otherwise, an empty stop_token is used (callback always invoked).
    ///
    /// @tparam TResult Type of the future result.
    /// @tparam TCallback Type of the callback receiver.
    /// @tparam CallbackMethod Type of the member function pointer.
    /// @param executor Executor to invoke the callback on.
    /// @param callbackObj Pointer to the callback receiver object.
    /// @param callbackMethod Pointer to the member function to invoke.
    /// @return CompletionCallback that can be passed to async methods.
    template <typename TResult, typename TCallback, typename CallbackMethod>
    CompletionCallback<TResult> CreateCallback(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
    {
      std::stop_token stopToken;

      // Automatically get stop token if available (SFINAE-friendly)
      if constexpr (requires { callbackObj->GetStopToken(); })
      {
        stopToken = callbackObj->GetStopToken();
      }

      return CreateCallback<TResult>(std::move(executor), callbackObj, callbackMethod, std::move(stopToken));
    }
  }    // namespace ServiceCallback
}

#endif
