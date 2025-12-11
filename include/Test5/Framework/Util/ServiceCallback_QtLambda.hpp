#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QTLAMBDA_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QTLAMBDA_HPP
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
#include <QObject>
#include <type_traits>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Attaches a lambda callback to a boost::future with Qt marshaling to a QObject's thread.
  ///
  /// The callback will be marshaled to the QObject's thread using Qt's event system
  /// (QMetaObject::invokeMethod with Qt::QueuedConnection) and will always be executed
  /// asynchronously (deferred). The lambda can capture state and doesn't require MOC or slots.
  ///
  /// Lifetime is managed by Qt's parent-child relationship and event queue - if the QObject
  /// is destroyed, the queued invocation will be safely dropped by Qt.
  ///
  /// Usage:
  /// @code
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::QtLambda(future, this, [this](boost::future<void> result) {
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
  /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
  /// @tparam TLambda Type of the lambda/callable.
  /// @param future The boost::future to attach the callback to.
  /// @param contextObj Pointer to the QObject for thread context and lifetime management.
  /// @param lambda The lambda or callable to invoke with the future result.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename TLambda>
  auto QtLambda(boost::future<TResult> future, TCallback* contextObj, TLambda&& lambda)
  {
    static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

    return future.then(
      [contextObj, lambda = std::forward<TLambda>(lambda)](boost::future<TResult> f)
      {
        // Marshal callback to QObject's thread using Qt's event system
        // Qt::QueuedConnection ensures the callback is always posted to the event queue
        // and never invoked synchronously, even if we're already on the target thread
        QMetaObject::invokeMethod(
          contextObj,
          [lambda = std::move(lambda), f = std::move(f)]() mutable
          {
            // Invoke the lambda on the QObject's thread
            lambda(std::move(f));
          },
          Qt::QueuedConnection);    // Always deferred - queued to event loop
      });
  }
}

#endif    // QT_VERSION

#endif
