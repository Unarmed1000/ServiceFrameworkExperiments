#ifndef SERVICE_FRAMEWORK_TEST3_QTSLOT_QTSLOTCALLBACKRECEIVER_HPP
#define SERVICE_FRAMEWORK_TEST3_QTSLOT_QTSLOTCALLBACKRECEIVER_HPP
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
#include <QObject>
#include <utility>
#include "ToFutureWithCallback.hpp"

namespace Test3
{
  namespace QtSlot
  {
    /// @brief Base class for QObjects that receive async callbacks using Qt slots.
    ///
    /// Provides convenient CallAsync helper methods that automatically pass 'this' and
    /// the stored executor. Requires Q_OBJECT macro and callback methods must be slots.
    ///
    /// This class leverages Qt's automatic cleanup - when the QObject is destroyed,
    /// Qt automatically handles pending queued connections safely.
    ///
    /// Thread-safe when used with Qt's queued connections model where callbacks are
    /// posted to the QObject's thread.
    class QtSlotCallbackReceiver : public QObject
    {
      Q_OBJECT

    public:
      /// @brief Constructs a callback receiver with the specified executor.
      /// @param executor The executor to run async operations on.
      /// @param parent Optional parent QObject for Qt's parent-child memory management.
      explicit QtSlotCallbackReceiver(boost::asio::any_io_executor executor, QObject* parent = nullptr)
        : QObject(parent)
        , m_executor(std::move(executor))
      {
      }

      /// @brief Gets the stored executor.
      /// @return The executor used for async operations.
      boost::asio::any_io_executor GetExecutor() const
      {
        return m_executor;
      }

      /// @brief Calls an async operation with callback (simple version).
      ///
      /// Convenience method that automatically passes 'this' and the stored executor.
      /// Returns only the future - the Qt connection is automatically managed.
      ///
      /// @tparam CallbackMethod Type of the callback slot method pointer.
      /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
      /// @param method Pointer to the callback slot method (e.g., &MyClass::handleResult).
      /// @param lambda Lambda that returns boost::asio::awaitable<T>.
      /// @return std::future<T> that can be used to wait for or retrieve the result.
      template <typename CallbackMethod, typename CoroutineLambda>
      auto CallAsync(CallbackMethod method, CoroutineLambda lambda)
      {
        return ToFutureWithCallback(m_executor, this, method, std::move(lambda));
      }

      /// @brief Calls an async operation with callback (connection-tracked version).
      ///
      /// Convenience method that automatically passes 'this' and the stored executor.
      /// Returns both the future and the Qt connection handle for explicit control.
      ///
      /// Use this when you need to explicitly disconnect the callback before the
      /// async operation completes (e.g., for cancellation scenarios).
      ///
      /// @tparam CallbackMethod Type of the callback slot method pointer.
      /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
      /// @param method Pointer to the callback slot method (e.g., &MyClass::handleResult).
      /// @param lambda Lambda that returns boost::asio::awaitable<T>.
      /// @return QtAsyncResult<T> containing both future and connection handle.
      template <typename CallbackMethod, typename CoroutineLambda>
      auto CallAsyncTracked(CallbackMethod method, CoroutineLambda lambda)
      {
        return ToFutureWithCallbackTracked(m_executor, this, method, std::move(lambda));
      }

    private:
      boost::asio::any_io_executor m_executor;
    };
  }    // namespace QtSlot
}    // namespace Test3

#endif
