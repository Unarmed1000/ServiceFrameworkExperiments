#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICEPROMISE_QTSIGNAL_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_SERVICEPROMISE_QTSIGNAL_HPP
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
#include <QPointer>
#include <memory>
#include <type_traits>
#include <utility>

namespace Test5::ServiceCallback
{
  /// @brief Connects to a Qt signal and wraps the result into a boost::promise/future.
  ///
  /// This function creates a one-shot connection to a Qt signal that will fulfill a boost::promise
  /// when the signal is emitted. The connection is automatically disconnected after the first
  /// emission. If the sender object is destroyed before the signal is emitted, the promise
  /// will be broken (future will throw broken_promise exception).
  ///
  /// The returned future can be used with boost::future continuations, allowing you to bridge
  /// from Qt's signal/slot mechanism to boost's async patterns.
  ///
  /// Usage:
  /// @code
  /// // For a signal with no parameters:
  /// auto future = ServiceCallback::ConnectSignalToPromise(myObject, &MyObject::finished);
  ///
  /// // For a signal with parameters:
  /// auto future = ServiceCallback::ConnectSignalToPromise<QString>(myObject, &MyObject::textChanged);
  /// @endcode
  ///
  /// @tparam TResult Type of the signal parameter (use void for signals with no parameters).
  /// @tparam TSender Type of the QObject that emits the signal.
  /// @tparam TSignal Type of the signal (member function pointer).
  /// @param sender Pointer to the QObject that will emit the signal.
  /// @param signal Pointer to the member function signal.
  /// @return A boost::future that will be fulfilled when the signal is emitted.
  template <typename TResult = void, typename TSender, typename TSignal>
  auto ConnectSignalToPromise(TSender* sender, TSignal signal)
  {
    static_assert(std::is_base_of_v<QObject, TSender>, "TSender must be derived from QObject");

    // Create a shared promise to be fulfilled when the signal is emitted
    auto promise = std::make_shared<boost::promise<TResult>>();
    auto future = promise->get_future();

    // Create QPointer for lifetime tracking
    QPointer<TSender> qptr(sender);

    // Helper object to manage the connection lifetime
    // This will be deleted automatically when the connection is triggered or when sender is destroyed
    auto* connectionHolder = new QObject();

    // Connect to the sender's destroyed signal to break the promise if sender is destroyed
    QObject::connect(sender, &QObject::destroyed, connectionHolder,
                     [promise, connectionHolder]()
                     {
                       // Sender destroyed before signal was emitted - break the promise
                       // (This will cause future.get() to throw boost::broken_promise)
                       connectionHolder->deleteLater();
                     });

    if constexpr (std::is_void_v<TResult>)
    {
      // For signals with no parameters
      QObject::connect(
        sender, signal, connectionHolder,
        [promise, connectionHolder, qptr]()
        {
          if (!qptr.isNull())
          {
            promise->set_value();
          }
          connectionHolder->deleteLater();    // Clean up and disconnect
        },
        Qt::QueuedConnection);
    }
    else
    {
      // For signals with parameters
      QObject::connect(
        sender, signal, connectionHolder,
        [promise, connectionHolder, qptr](TResult result)
        {
          if (!qptr.isNull())
          {
            promise->set_value(std::move(result));
          }
          connectionHolder->deleteLater();    // Clean up and disconnect
        },
        Qt::QueuedConnection);
    }

    return future;
  }
}

#endif    // QT_VERSION

#endif
