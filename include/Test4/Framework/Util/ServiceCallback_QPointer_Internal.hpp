#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_INTERNAL_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_SERVICECALLBACK_QPOINTER_INTERNAL_HPP
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
#include <QPointer>
#include <future>
#include <utility>

namespace Test4
{
  namespace ServiceCallback
  {
    namespace Internal
    {
      /// @brief Implementation of QPointer-based callback with automatic lifetime tracking.
      ///
      /// Uses QPointer to safely check if the callback QObject still exists before
      /// invoking the callback. Provides automatic protection against use-after-free
      /// without requiring Qt's MOC or slot declarations.
      ///
      /// The callback is invoked on the QObject's thread using Qt's queued connection
      /// mechanism for thread-safe marshaling.
      template <typename TResult, typename TCallback, typename CallbackMethod>
      class QPointerCallbackImpl final : public CompletionCallback<TResult>::ICallbackImpl
      {
        boost::asio::any_io_executor m_executor;
        QPointer<TCallback> m_callbackPtr;
        CallbackMethod m_callbackMethod;

      public:
        QPointerCallbackImpl(boost::asio::any_io_executor executor, TCallback* callbackObj, CallbackMethod callbackMethod)
          : m_executor(std::move(executor))
          , m_callbackPtr(callbackObj)
          , m_callbackMethod(callbackMethod)
        {
        }

        void Invoke(std::future<TResult> result) override
        {
          // Check if object still exists before invoking callback
          if (!m_callbackPtr.isNull())
          {
            // Capture QPointer and data for the queued invocation
            auto callbackPtr = m_callbackPtr;
            auto callbackMethod = m_callbackMethod;

            // Invoke callback on QObject's thread using Qt's queued connection
            QMetaObject::invokeMethod(
              m_callbackPtr.data(),
              [callbackPtr, callbackMethod, result = std::move(result)]() mutable
              {
                // Double-check in case object was destroyed between queue and execution
                if (!callbackPtr.isNull())
                {
                  // Invoke callback method with future
                  (callbackPtr.data()->*callbackMethod)(std::move(result));
                }
              },
              Qt::QueuedConnection);
          }
        }
      };
    }    // namespace Internal
  }    // namespace ServiceCallback
}    // namespace Test4

#endif    // QT_VERSION

#endif
