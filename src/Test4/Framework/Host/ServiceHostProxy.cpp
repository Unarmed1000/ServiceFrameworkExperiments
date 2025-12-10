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

#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test4/Framework/Host/ServiceHostProxy.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <exception>
#include <memory>
#include "../../Test2/Framework/Host/ServiceHostBase.hpp"

namespace Test4
{
  namespace
  {
    /// @brief Helper to invoke a ServiceHostBase method and return result via std::future.
    ///
    /// This function marshals the call to the target executor, invokes the method,
    /// captures the result in a std::promise, and optionally invokes a callback
    /// on the source executor when complete.
    ///
    /// @tparam TResult Return type of the ServiceHostBase method.
    /// @tparam TMethod Type of the method pointer.
    /// @tparam TArgs Types of method arguments.
    /// @param dispatchContext Dispatch context with source and target executors.
    /// @param method Pointer to ServiceHostBase method to invoke.
    /// @param callback Optional completion callback.
    /// @param args Arguments to pass to the method.
    /// @return std::future<TResult> that will contain the result or exception.
    template <typename TResult, typename TMethod, typename... TArgs>
    std::future<TResult> InvokeWithFuture(const Test2::DispatchContext<Test2::ILifeTracker, Test2::ServiceHostBase>& dispatchContext, TMethod method,
                                          CompletionCallback<TResult> callback, TArgs&&... args)
    {
      auto promise = std::make_shared<std::promise<TResult>>();
      auto future = promise->get_future();

      auto targetContext = dispatchContext.GetTargetContext();
      auto sourceContext = dispatchContext.GetSourceContext();

      // Co-spawn the work on the target executor
      boost::asio::co_spawn(
        targetContext.GetExecutor(),
        [promise, targetContext, sourceContext, method, callback = std::move(callback),
         ... args = std::forward<TArgs>(args)]() mutable -> boost::asio::awaitable<void>
        {
          try
          {
            // Check if target is still alive
            auto target = targetContext.TryLock();
            if (!target)
            {
              // Target destroyed - set exception
              promise->set_exception(std::make_exception_ptr(std::runtime_error("ServiceHostBase has been destroyed")));
            }
            else
            {
              // Invoke the method on the target
              if constexpr (std::is_void_v<TResult>)
              {
                co_await (target.get()->*method)(std::forward<TArgs>(args)...);
                promise->set_value();
              }
              else
              {
                auto result = co_await (target.get()->*method)(std::forward<TArgs>(args)...);
                promise->set_value(std::move(result));
              }
            }
          }
          catch (...)
          {
            promise->set_exception(std::current_exception());
          }

          // Invoke callback on source executor if provided and source is alive
          if (callback && sourceContext.IsAlive())
          {
            boost::asio::post(sourceContext.GetExecutor(),
                              [promise, callback = std::move(callback)]() mutable { callback.Invoke(promise->get_future()); });
          }
        },
        boost::asio::detached);

      return future;
    }

    /// @brief Helper to invoke a non-coroutine ServiceHostBase method and return result via std::future.
    ///
    /// Similar to InvokeWithFuture but for methods that return regular values, not awaitables.
    ///
    /// @tparam TResult Return type of the ServiceHostBase method.
    /// @tparam TMethod Type of the method pointer.
    /// @tparam TArgs Types of method arguments.
    /// @param dispatchContext Dispatch context with source and target executors.
    /// @param method Pointer to ServiceHostBase method to invoke.
    /// @param callback Optional completion callback.
    /// @param args Arguments to pass to the method.
    /// @return std::future<TResult> that will contain the result or exception.
    template <typename TResult, typename TMethod, typename... TArgs>
    std::future<TResult> InvokeNonCoroutineWithFuture(const Test2::DispatchContext<Test2::ILifeTracker, Test2::ServiceHostBase>& dispatchContext,
                                                      TMethod method, CompletionCallback<TResult> callback, TArgs&&... args)
    {
      auto promise = std::make_shared<std::promise<TResult>>();
      auto future = promise->get_future();

      auto targetContext = dispatchContext.GetTargetContext();
      auto sourceContext = dispatchContext.GetSourceContext();

      // Post the work to the target executor
      boost::asio::post(
        targetContext.GetExecutor(),
        [promise, targetContext, sourceContext, method, callback = std::move(callback), ... args = std::forward<TArgs>(args)]() mutable
        {
          try
          {
            // Check if target is still alive
            auto target = targetContext.TryLock();
            if (!target)
            {
              // Target destroyed - set exception
              promise->set_exception(std::make_exception_ptr(std::runtime_error("ServiceHostBase has been destroyed")));
            }
            else
            {
              // Invoke the method on the target
              if constexpr (std::is_void_v<TResult>)
              {
                (target.get()->*method)(std::forward<TArgs>(args)...);
                promise->set_value();
              }
              else
              {
                auto result = (target.get()->*method)(std::forward<TArgs>(args)...);
                promise->set_value(std::move(result));
              }
            }
          }
          catch (...)
          {
            promise->set_exception(std::current_exception());
          }

          // Invoke callback on source executor if provided and source is alive
          if (callback && sourceContext.IsAlive())
          {
            boost::asio::post(sourceContext.GetExecutor(),
                              [promise, callback = std::move(callback)]() mutable { callback.Invoke(promise->get_future()); });
          }
        });

      return future;
    }
  }

  ServiceHostProxy::ServiceHostProxy(Test2::DispatchContext<Test2::ILifeTracker, Test2::ServiceHostBase> dispatchContext)
    : m_dispatchContext(std::move(dispatchContext))
  {
  }

  ServiceHostProxy::~ServiceHostProxy() = default;

  std::future<void> ServiceHostProxy::TryStartServicesAsync(std::vector<Test2::StartServiceRecord> services,
                                                            const Test2::ServiceLaunchPriority currentPriority, CompletionCallback<void> callback)
  {
    return InvokeWithFuture<void>(m_dispatchContext, &Test2::ServiceHostBase::TryStartServicesAsync, std::move(callback), std::move(services),
                                  currentPriority);
  }

  std::future<std::vector<std::exception_ptr>>
    ServiceHostProxy::TryShutdownServicesAsync(const Test2::ServiceLaunchPriority priority,
                                               CompletionCallback<std::vector<std::exception_ptr>> callback)
  {
    return InvokeWithFuture<std::vector<std::exception_ptr>>(m_dispatchContext, &Test2::ServiceHostBase::TryShutdownServicesAsync,
                                                             std::move(callback), priority);
  }

  std::future<bool> ServiceHostProxy::TryRequestShutdownAsync(CompletionCallback<bool> callback)
  {
    // RequestShutdown returns void, but we want to return bool indicating success
    // We'll use a lambda to adapt it
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    auto targetContext = m_dispatchContext.GetTargetContext();
    auto sourceContext = m_dispatchContext.GetSourceContext();

    boost::asio::post(targetContext.GetExecutor(),
                      [promise, targetContext, sourceContext, callback = std::move(callback)]() mutable
                      {
                        try
                        {
                          auto target = targetContext.TryLock();
                          if (!target)
                          {
                            promise->set_value(false);    // Target destroyed
                          }
                          else
                          {
                            target->RequestShutdown();
                            promise->set_value(true);    // Success
                          }
                        }
                        catch (...)
                        {
                          promise->set_exception(std::current_exception());
                        }

                        // Invoke callback on source executor if provided and source is alive
                        if (callback && sourceContext.IsAlive())
                        {
                          boost::asio::post(sourceContext.GetExecutor(),
                                            [promise, callback = std::move(callback)]() mutable { callback.Invoke(promise->get_future()); });
                        }
                      });

    return future;
  }

  bool ServiceHostProxy::TryRequestShutdown() noexcept
  {
    try
    {
      auto targetContext = m_dispatchContext.GetTargetContext();
      auto target = targetContext.TryLock();

      if (!target)
      {
        return false;    // Target destroyed
      }

      boost::asio::post(targetContext.GetExecutor(), [target]() { target->RequestShutdown(); });

      return true;
    }
    catch (...)
    {
      return false;
    }
  }
}
