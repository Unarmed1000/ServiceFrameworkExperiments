#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_HPP
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
#include <QMetaObject>
#include <QObject>
#include <type_traits>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Attaches a callback to a boost::future using Qt's slot mechanism.
  ///
  /// The callback will be invoked on the QObject's thread. The callback method
  /// must be declared as a slot (with Q_OBJECT macro and slots keyword).
  ///
  /// Lifetime is managed by Qt's parent-child relationship - if the QObject is
  /// destroyed, the queued invocation will be safely dropped by Qt.
  ///
  /// Usage:
  /// @code
  /// // In class declaration:
  /// class MyQObject : public QObject {
  ///   Q_OBJECT
  /// public slots:
  ///   void OnComplete(boost::future<void> future);
  /// };
  ///
  /// // Usage:
  /// auto future = proxy.TryStartServicesAsync(services, priority);
  /// ServiceCallback::CreateQtSlotCallback(future, executor, this, &MyQObject::OnComplete);
  /// @endcode
  ///
  /// @tparam TResult Type of the future result.
  /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
  /// @tparam CallbackMethod Type of the slot method pointer.
  /// @param future The boost::future to attach the callback to.
  /// @param executor Executor for the async work (not used for callback marshaling - Qt handles that).
  /// @param callbackObj Pointer to the QObject callback receiver.
  /// @param callbackMethod Pointer to the slot method to invoke.
  /// @return A new boost::future representing the continuation.
  template <typename TResult, typename TCallback, typename CallbackMethod>
  auto CreateQtSlotCallback(boost::future<TResult> future, [[maybe_unused]] boost::asio::any_io_executor executor, TCallback* callbackObj,
                            CallbackMethod callbackMethod)
  {
    static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

    return future.then(
      [callbackObj, callbackMethod](boost::future<TResult> f) mutable
      {
        // Use Qt's invokeMethod to queue the call on the object's thread
        // Qt::QueuedConnection ensures thread safety and handles object lifetime
        QMetaObject::invokeMethod(
          callbackObj,
          [callbackObj, callbackMethod, f = std::move(f)]() mutable
          {
            // Invoke the slot method
            (callbackObj->*callbackMethod)(std::move(f));
          },
          Qt::QueuedConnection);
      });
  }
}

#endif    // QT_VERSION

#endif
