#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_METHOD_INTERNAL_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_METHOD_INTERNAL_HPP
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
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <future>
#include <stop_token>
#include <type_traits>
#include <utility>

namespace Test4
{
  namespace ServiceCallback
  {
    namespace Internal
    {
      /// @brief Implementation of method callback with stop_token lifetime tracking.
      ///
      /// Invokes a member function on the callback receiver when the operation completes.
      /// Checks stop_token before invoking to ensure the object is still alive.
      template <typename TResult, typename TCallback, typename CallbackMethod>
      class MethodCallbackImpl final : public CompletionCallback<TResult>::ICallbackImpl
      {
        boost::asio::any_io_executor m_executor;
        TCallback* m_callbackObj;
        CallbackMethod m_callbackMethod;
        std::stop_token m_stopToken;

      public:
        MethodCallbackImpl(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod, std::stop_token stopToken)
          : m_executor(std::move(executor))
          , m_callbackObj(callbackObj)
          , m_callbackMethod(callbackMethod)
          , m_stopToken(std::move(stopToken))
        {
        }

        void Invoke(std::future<TResult> result) override
        {
          // Post to the callback's executor with lifetime checking
          boost::asio::post(
            m_executor,
            [callbackObj = m_callbackObj, callbackMethod = m_callbackMethod, stopToken = m_stopToken, result = std::move(result)]() mutable
            {
              // Check stop token before invoking callback
              if constexpr (requires { stopToken.stop_requested(); })
              {
                if (stopToken.stop_requested())
                {
                  return;    // Object is being destroyed - skip callback
                }
              }

              // Invoke the callback method with the future
              (callbackObj->*callbackMethod)(std::move(result));
            });
        }
      };
    }    // namespace Internal
  }    // namespace ServiceCallback
}    // namespace Test4

#endif
