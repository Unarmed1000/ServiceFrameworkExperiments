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

#include <boost/thread/future.hpp>
#include <QMetaObject>
#include <QPointer>
#include <type_traits>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Attaches a callback to a boost::future with QPointer lifetime tracking and Qt marshaling.
  ///
  /// The callback will be marshaled to the QObject's thread using Qt's event system
  /// (QMetaObject::invokeMethod with Qt::QueuedConnection). The callback will only execute
  /// if the QObject still exists (QPointer is not null), providing automatic thread-safe
  /// marshaling and protection against use-after-free without requiring Qt's MOC or slot
  /// declarations.
  ///
  /// The callback is always executed asynchronously (deferred) on the thread associated with
  /// the QObject (typically the thread where it was created).
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::CreateQPointer(future, this, &MyQObject::OnComplete);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
  /// @tparam CallbackMethod Type of the member function pointer.
  /// @param future The boost::future to attach the callback to.
  /// @param callbackObj Pointer to the QObject callback receiver.
  /// @param callbackMethod Pointer to the member function to invoke.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename CallbackMethod>
  auto CreateQPointer(boost::future<TResult> future, TCallback* callbackObj, CallbackMethod callbackMethod)
  {
    static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

    // Create QPointer for lifetime tracking
    QPointer<TCallback> qptr(callbackObj);

    return future.then(
      [qptr, callbackMethod](boost::future<TResult> f)
      {
        // Check if QObject is still alive
        if (qptr.isNull())
        {
          return;    // Object destroyed, skip callback
        }

        // Marshal callback to QObject's thread using Qt's event system
        // Qt::QueuedConnection ensures the callback is always posted to the event queue
        // and never invoked synchronously, even if we're already on the target thread
        QMetaObject::invokeMethod(
          qptr.data(),
          [qptr, callbackMethod, f = std::move(f)]() mutable
          {
            // Double-check if object is still alive at execution time
            if (qptr.isNull())
            {
              return;    // Object destroyed, skip callback
            }

            // Invoke the callback method on the QObject's thread
            (qptr.data()->*callbackMethod)(std::move(f));
          },
          Qt::QueuedConnection);    // Always deferred - queued to event loop
      });
  }
}

#endif    // QT_VERSION

#endif
