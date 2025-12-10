#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_INTERNAL_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QTSLOT_INTERNAL_HPP
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
#include <utility>

namespace Test4
{
  namespace ServiceCallback
  {
    namespace Internal
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
    }    // namespace Internal
  }    // namespace ServiceCallback
}    // namespace Test4

#endif    // QT_VERSION

#endif
