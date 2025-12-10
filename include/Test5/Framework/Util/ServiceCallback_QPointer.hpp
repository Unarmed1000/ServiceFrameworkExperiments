#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_HPP
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

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/thread/future.hpp>
#include <QPointer>
#include <type_traits>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Attaches a callback to a boost::future with QPointer lifetime tracking.
  ///
  /// The callback will be marshaled to the specified executor and will only execute
  /// if the QObject still exists (QPointer is not null). This provides automatic
  /// protection against use-after-free without requiring Qt's MOC or slot declarations.
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::CreateQPointerCallback(future, executor, this, &MyQObject::OnComplete);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
  /// @tparam CallbackMethod Type of the member function pointer.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor to invoke the callback on.
  /// @param callbackObj Pointer to the QObject callback receiver.
  /// @param callbackMethod Pointer to the member function to invoke.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename CallbackMethod>
  auto CreateQPointerCallback(boost::future<TResult> future, boost::asio::any_io_executor executor, TCallback* callbackObj,
                              CallbackMethod callbackMethod)
  {
    static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

    // Create QPointer for lifetime tracking
    QPointer<TCallback> qptr(callbackObj);

    return future.then(
      [executor = std::move(executor), qptr, callbackMethod](boost::future<TResult> f) mutable
      {
        // Check if QObject is still alive before posting
        if (qptr.isNull())
        {
          return;    // Object destroyed, skip callback
        }

        // Marshal callback to the specified executor
        boost::asio::post(executor,
                          [qptr, callbackMethod, f = std::move(f)]() mutable
                          {
                            // Double-check if object is still alive at execution time
                            if (qptr.isNull())
                            {
                              return;    // Object destroyed, skip callback
                            }

                            // Invoke the callback method
                            (qptr.data()->*callbackMethod)(std::move(f));
                          });
      });
  }
}

#endif    // QT_VERSION

#endif
