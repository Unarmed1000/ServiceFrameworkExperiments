#ifndef SERVICE_FRAMEWORK_TEST3_QTPOINTER_QTPOINTERCALLBACKRECEIVER_HPP
#define SERVICE_FRAMEWORK_TEST3_QTPOINTER_QTPOINTERCALLBACKRECEIVER_HPP
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
  namespace QtPointer
  {
    /// @brief Base class for QObjects that receive async callbacks without MOC requirements.
    ///
    /// Provides convenient CallAsync helper method that automatically passes 'this' and
    /// the stored executor. Does NOT require Q_OBJECT macro or slots declarations.
    ///
    /// This class leverages QPointer for automatic lifetime safety - callbacks are
    /// automatically skipped if the QObject is destroyed before the async operation
    /// completes.
    ///
    /// Thread-safe when used with Qt's queued connections model where callbacks are
    /// posted to the QObject's thread.
    ///
    /// NOTE: This class does NOT use Q_OBJECT, making it suitable for header-only
    /// usage and avoiding MOC dependencies. Callback methods are regular member
    /// functions, not Qt slots.
    class QtPointerCallbackReceiver : public QObject
    {
    public:
      /// @brief Constructs a callback receiver with the specified executor.
      /// @param executor The executor to run async operations on.
      /// @param parent Optional parent QObject for Qt's parent-child memory management.
      explicit QtPointerCallbackReceiver(boost::asio::any_io_executor executor, QObject* parent = nullptr)
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

      /// @brief Calls an async operation with callback.
      ///
      /// Convenience method that automatically passes 'this' and the stored executor.
      /// The callback is automatically skipped if this object is destroyed before the
      /// async operation completes (via QPointer null check).
      ///
      /// @tparam CallbackMethod Type of the callback member function pointer.
      /// @tparam CoroutineLambda Type of the lambda returning awaitable<T>.
      /// @param method Pointer to the callback member function (e.g., &MyClass::handleResult).
      /// @param lambda Lambda that returns boost::asio::awaitable<T>.
      /// @return std::future<T> that can be used to wait for or retrieve the result.
      template <typename CallbackMethod, typename CoroutineLambda>
      auto CallAsync(CallbackMethod method, CoroutineLambda lambda)
      {
        return ToFutureWithCallback(m_executor, this, method, std::move(lambda));
      }

    private:
      boost::asio::any_io_executor m_executor;
    };
  }    // namespace QtPointer
}    // namespace Test3

#endif
