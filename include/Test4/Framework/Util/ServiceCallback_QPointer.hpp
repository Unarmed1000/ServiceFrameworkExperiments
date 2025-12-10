#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_HPP
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

#ifdef QT_VERSION

#include <Test4/Framework/Util/CompletionCallback.hpp>
#include <Test4/Framework/Util/ServiceCallback_QPointer_Internal.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <future>
#include <memory>
#include <type_traits>

namespace Test4
{
  namespace ServiceCallback
  {
    /// @brief Creates a QPointer-based callback with automatic lifetime tracking.
    ///
    /// The callback will be invoked on the QObject's thread and will only execute
    /// if the QObject still exists (QPointer is not null). This provides automatic
    /// protection against use-after-free without requiring Qt's MOC or slot declarations.
    ///
    /// @tparam TResult Type of the future result.
    /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
    /// @tparam CallbackMethod Type of the member function pointer.
    /// @param executor Executor for the async work (not used for callback marshaling - Qt handles that).
    /// @param callbackObj Pointer to the QObject callback receiver.
    /// @param callbackMethod Pointer to the member function to invoke.
    /// @return CompletionCallback that can be passed to async methods.
    template <typename TResult, typename TCallback, typename CallbackMethod>
    CompletionCallback<TResult> CreateQPointerCallback(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
    {
      static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

      auto impl =
        std::make_unique<Internal::QPointerCallbackImpl<TResult, TCallback, CallbackMethod>>(std::move(executor), callbackObj, callbackMethod);

      return CompletionCallback<TResult>(std::move(impl));
    }
  }    // namespace ServiceCallback
}

#endif    // QT_VERSION

#endif
