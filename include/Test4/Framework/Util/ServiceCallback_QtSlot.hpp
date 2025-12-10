#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_HPP
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
#include <boost/asio/any_io_executor.hpp>
#include <QMetaObject>
#include <QObject>
#include <future>
#include <memory>
#include <utility>

namespace Test4
{
  namespace ServiceCallback
  {
    /// @brief Implementation of Qt slot-based callback with connection tracking.
    ///
    /// Invokes a Qt slot on the callback QObject when the operation completes.
    /// The slot must be declared with the Q_OBJECT macro and slots keyword.
    ///
    /// The callback is invoked on the QObject's thread using Qt's queued connection
    /// mechanism. The QObject's parent-child relationship provides automatic cleanup
    /// if the object is destroyed.
    template <typename TResult, typename TCallback, typename CallbackMethod>
    class QtSlotCallbackImpl final : public CompletionCallback<TResult>::ICallbackImpl
    {
      boost::asio::any_io_executor m_executor;
      TCallback* m_callbackObj;
      CallbackMethod m_callbackMethod;

    public:
      QtSlotCallbackImpl(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
        : m_executor(std::move(executor))
        , m_callbackObj(callbackObj)
        , m_callbackMethod(callbackMethod)
      {
      }

      void Invoke(std::future<TResult> result) override
      {
        // Invoke callback slot on QObject's thread using Qt's queued connection
        QMetaObject::invokeMethod(
          m_callbackObj,
          [callbackObj = m_callbackObj, callbackMethod = m_callbackMethod, result = std::move(result)]() mutable
          {
            // Invoke callback slot with future
            (callbackObj->*callbackMethod)(std::move(result));
          },
          Qt::QueuedConnection);
      }
    };

    /// @brief Creates a Qt slot-based callback.
    ///
    /// The callback will be invoked on the QObject's thread. The callback method
    /// must be declared as a slot (with Q_OBJECT macro and slots keyword).
    ///
    /// Lifetime is managed by Qt's parent-child relationship - if the QObject is
    /// destroyed, the queued invocation will be safely dropped by Qt.
    ///
    /// @tparam TResult Type of the future result.
    /// @tparam TCallback Type of the callback receiver (must be QObject-derived).
    /// @tparam CallbackMethod Type of the slot method pointer.
    /// @param executor Executor for the async work (not used for callback marshaling - Qt handles that).
    /// @param callbackObj Pointer to the QObject callback receiver.
    /// @param callbackMethod Pointer to the slot method to invoke.
    /// @return CompletionCallback that can be passed to async methods.
    template <typename TResult, typename TCallback, typename CallbackMethod>
    CompletionCallback<TResult> CreateQtSlotCallback(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
    {
      static_assert(std::is_base_of_v<QObject, TCallback>, "TCallback must be derived from QObject");

      auto impl = std::make_unique<QtSlotCallbackImpl<TResult, TCallback, CallbackMethod>>(std::move(executor), callbackObj, callbackMethod);

      return CompletionCallback<TResult>(std::move(impl));
    }
  }    // namespace ServiceCallback
}

#endif    // QT_VERSION

#endif
