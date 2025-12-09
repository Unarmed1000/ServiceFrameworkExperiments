#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_UTIL_ASYNCPROXYHELPER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_UTIL_ASYNCPROXYHELPER_HPP
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

#include <Test2/Framework/Exception/ServiceDisposedException.hpp>
#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Test2
{
  /// @brief Empty debug hint for default template parameter.
  inline constexpr const char kEmptyDebugHint[] = "";

  namespace Util
  {

    namespace Detail
    {
      // Helper to detect if a type is boost::asio::awaitable<T>
      template <typename>
      struct is_awaitable : std::false_type
      {
      };

      template <typename T>
      struct is_awaitable<boost::asio::awaitable<T>> : std::true_type
      {
        using value_type = T;
      };

      template <typename T>
      inline constexpr bool is_awaitable_v = is_awaitable<T>::value;

      template <typename T>
      using awaitable_value_t = typename is_awaitable<T>::value_type;
    }    // namespace Detail

    // ========================================================================================================
    // ExecutorContext-based API
    // ========================================================================================================

    /// @brief Invokes a member function on an ExecutorContext-managed object via co_spawn, throwing on expiration.
    ///
    /// This helper abstracts the common pattern of:
    /// 1. Locking a weak_ptr to check if the target object is still alive
    /// 2. Throwing ServiceDisposedException if expired
    /// 3. Spawning a coroutine on the target executor
    /// 4. Invoking the member function with forwarded arguments
    ///
    /// Handles both regular member functions and member functions that return awaitable<T>.
    ///
    /// @tparam DebugHintName Optional debug hint for exception messages (compile-time const char*).
    /// @tparam T Type of the object managed by the ExecutorContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param context The executor context containing the executor and weak_ptr.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable that completes with the result of the member function invocation.
    /// @throws ServiceDisposedException if the weak_ptr is expired.
    template <const char* DebugHintName = kEmptyDebugHint, typename T, typename MemberFunc, typename... Args>
    auto InvokeAsync(const Lifecycle::ExecutorContext<T>& context, MemberFunc memberFunc, Args&&... args)
    {
      using RawResultType = std::invoke_result_t<MemberFunc, T*, std::decay_t<Args>...>;
      auto executor = context.GetExecutor();
      auto weakPtr = context.GetWeakPtr();

      // Check if the member function returns an awaitable
      if constexpr (Detail::is_awaitable_v<RawResultType>)
      {
        // Member function returns awaitable<U>, extract U
        using ResultType = Detail::awaitable_value_t<RawResultType>;

        return boost::asio::co_spawn(
          executor,
          [weakPtr, func = std::mem_fn(memberFunc), ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            auto ptr = weakPtr.lock();
            if (!ptr)
            {
              throw ServiceDisposedException(DebugHintName);
            }

            // Invoke returns awaitable, so we need to co_await it
            co_return co_await func(ptr, std::move(args)...);
          },
          boost::asio::use_awaitable);
      }
      else
      {
        // Member function returns regular type
        using ResultType = RawResultType;

        return boost::asio::co_spawn(
          executor,
          [weakPtr, func = std::mem_fn(memberFunc), ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            auto ptr = weakPtr.lock();
            if (!ptr)
            {
              throw ServiceDisposedException(DebugHintName);
            }

            if constexpr (std::is_void_v<ResultType>)
            {
              func(ptr, std::move(args)...);
              co_return;
            }
            else
            {
              co_return func(ptr, std::move(args)...);
            }
          },
          boost::asio::use_awaitable);
      }
    }

    /// @brief Invokes a member function on an ExecutorContext-managed object, returning to the calling executor, throwing on expiration.
    ///
    /// This variant accepts a callingExecutor and an ExecutorContext for the target. After the
    /// operation completes on the target executor, execution resumes on the calling executor.
    ///
    /// Handles both regular member functions and member functions that return awaitable<T>.
    ///
    /// @tparam DebugHintName Optional debug hint for exception messages (compile-time const char*).
    /// @tparam T Type of the object managed by the ExecutorContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param callingExecutor The executor to return to after the operation completes.
    /// @param targetContext The executor context for the target object.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable that completes with the result of the member function invocation.
    /// @throws ServiceDisposedException if the weak_ptr is expired. Resumes on callingExecutor.
    template <const char* DebugHintName = kEmptyDebugHint, typename T, typename MemberFunc, typename... Args>
    auto InvokeAsync(boost::asio::any_io_executor callingExecutor, const Lifecycle::ExecutorContext<T>& targetContext, MemberFunc memberFunc,
                     Args&&... args)
    {
      using RawResultType = std::invoke_result_t<MemberFunc, T*, std::decay_t<Args>...>;
      auto targetExecutor = targetContext.GetExecutor();
      auto weakPtr = targetContext.GetWeakPtr();

      // Check if the member function returns an awaitable
      if constexpr (Detail::is_awaitable_v<RawResultType>)
      {
        // Member function returns awaitable<U>, extract U
        using ResultType = Detail::awaitable_value_t<RawResultType>;

        return boost::asio::co_spawn(
          callingExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ResultType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  throw ServiceDisposedException(DebugHintName);
                }

                // Invoke returns awaitable, so we need to co_await it
                co_return co_await func(ptr, std::move(args)...);
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            co_return result;
          },
          boost::asio::use_awaitable);
      }
      else
      {
        // Member function returns regular type
        using ResultType = RawResultType;

        return boost::asio::co_spawn(
          callingExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ResultType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  throw ServiceDisposedException(DebugHintName);
                }

                if constexpr (std::is_void_v<ResultType>)
                {
                  func(ptr, std::move(args)...);
                  co_return;
                }
                else
                {
                  co_return func(ptr, std::move(args)...);
                }
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            if constexpr (!std::is_void_v<ResultType>)
            {
              co_return result;
            }
          },
          boost::asio::use_awaitable);
      }
    }

    /// @brief Invokes a member function on an ExecutorContext-managed object, returning optional on expiration.
    ///
    /// This is the non-throwing variant of InvokeAsync. Instead of throwing when the weak_ptr is expired,
    /// it returns std::nullopt (for non-void functions) or false (for void functions).
    ///
    /// Handles both regular member functions and member functions that return awaitable<T>.
    ///
    /// @tparam DebugHintName Optional debug hint (unused in non-throwing variant, kept for consistency).
    /// @tparam T Type of the object managed by the ExecutorContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param context The executor context containing the executor and weak_ptr.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable<std::optional<ResultType>> for non-void functions, or awaitable<bool> for void functions.
    ///         Returns std::nullopt or false if the weak_ptr is expired.
    template <const char* DebugHintName = kEmptyDebugHint, typename T, typename MemberFunc, typename... Args>
    auto TryInvokeAsync(const Lifecycle::ExecutorContext<T>& context, MemberFunc memberFunc, Args&&... args)
    {
      using RawResultType = std::invoke_result_t<MemberFunc, T*, std::decay_t<Args>...>;
      auto executor = context.GetExecutor();
      auto weakPtr = context.GetWeakPtr();

      // Check if the member function returns an awaitable
      if constexpr (Detail::is_awaitable_v<RawResultType>)
      {
        // Member function returns awaitable<U>, extract U
        using ResultType = Detail::awaitable_value_t<RawResultType>;
        using ReturnType = std::conditional_t<std::is_void_v<ResultType>, bool, std::optional<ResultType>>;

        return boost::asio::co_spawn(
          executor,
          [weakPtr, func = std::mem_fn(memberFunc), ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ReturnType>
          {
            auto ptr = weakPtr.lock();
            if (!ptr)
            {
              if constexpr (std::is_void_v<ResultType>)
              {
                co_return false;
              }
              else
              {
                co_return std::nullopt;
              }
            }

            if constexpr (std::is_void_v<ResultType>)
            {
              co_await func(ptr, std::move(args)...);
              co_return true;
            }
            else
            {
              co_return std::optional<ResultType>(co_await func(ptr, std::move(args)...));
            }
          },
          boost::asio::use_awaitable);
      }
      else
      {
        // Member function returns regular type
        using ResultType = RawResultType;
        using ReturnType = std::conditional_t<std::is_void_v<ResultType>, bool, std::optional<ResultType>>;

        return boost::asio::co_spawn(
          executor,
          [weakPtr, func = std::mem_fn(memberFunc), ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ReturnType>
          {
            auto ptr = weakPtr.lock();
            if (!ptr)
            {
              if constexpr (std::is_void_v<ResultType>)
              {
                co_return false;
              }
              else
              {
                co_return std::nullopt;
              }
            }

            if constexpr (std::is_void_v<ResultType>)
            {
              func(ptr, std::move(args)...);
              co_return true;
            }
            else
            {
              co_return std::optional<ResultType>(func(ptr, std::move(args)...));
            }
          },
          boost::asio::use_awaitable);
      }
    }

    /// @brief Invokes a member function using a DispatchContext, throwing on expiration.
    ///
    /// This helper uses a DispatchContext to automatically extract both source and target executors.
    /// After the operation completes on the target executor, execution resumes on the source executor.
    ///
    /// @tparam DebugHintName Optional debug hint for exception messages (compile-time const char*).
    /// @tparam TSource Type of the source object in the DispatchContext.
    /// @tparam TTarget Type of the target object in the DispatchContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param dispatchContext The dispatch context containing source and target executor contexts.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable that completes with the result of the member function invocation.
    /// @throws ServiceDisposedException if the target weak_ptr is expired.
    template <const char* DebugHintName = kEmptyDebugHint, typename TSource, typename TTarget, typename MemberFunc, typename... Args>
    auto InvokeAsync(const Lifecycle::DispatchContext<TSource, TTarget>& dispatchContext, MemberFunc memberFunc, Args&&... args)
    {
      return InvokeAsync<DebugHintName>(dispatchContext.GetSourceExecutor(), dispatchContext.GetTargetContext(), std::move(memberFunc),
                                        std::forward<Args>(args)...);
    }

    /// @brief Invokes a member function on an ExecutorContext-managed object, returning to the calling executor.
    ///
    /// This variant accepts a callingExecutor and an ExecutorContext for the target. After the
    /// operation completes on the target executor, execution resumes on the calling executor.
    ///
    /// Handles both regular member functions and member functions that return awaitable<T>.
    ///
    /// @tparam DebugHintName Optional debug hint (unused in non-throwing variant, kept for consistency).
    /// @tparam T Type of the object managed by the ExecutorContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param callingExecutor The executor to return to after the operation completes.
    /// @param targetContext The executor context for the target object.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable<std::optional<ResultType>> for non-void functions, or awaitable<bool> for void functions.
    ///         Returns std::nullopt or false if the weak_ptr is expired. Resumes on callingExecutor.
    template <const char* DebugHintName = kEmptyDebugHint, typename T, typename MemberFunc, typename... Args>
    auto TryInvokeAsync(boost::asio::any_io_executor callingExecutor, const Lifecycle::ExecutorContext<T>& targetContext, MemberFunc memberFunc,
                        Args&&... args)
    {
      using RawResultType = std::invoke_result_t<MemberFunc, T*, std::decay_t<Args>...>;
      auto targetExecutor = targetContext.GetExecutor();
      auto weakPtr = targetContext.GetWeakPtr();

      // Check if the member function returns an awaitable
      if constexpr (Detail::is_awaitable_v<RawResultType>)
      {
        // Member function returns awaitable<U>, extract U
        using ResultType = Detail::awaitable_value_t<RawResultType>;
        using ReturnType = std::conditional_t<std::is_void_v<ResultType>, bool, std::optional<ResultType>>;

        return boost::asio::co_spawn(
          callingExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ReturnType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ReturnType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  if constexpr (std::is_void_v<ResultType>)
                  {
                    co_return false;
                  }
                  else
                  {
                    co_return std::nullopt;
                  }
                }

                if constexpr (std::is_void_v<ResultType>)
                {
                  co_await func(ptr, std::move(args)...);
                  co_return true;
                }
                else
                {
                  co_return std::optional<ResultType>(co_await func(ptr, std::move(args)...));
                }
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            co_return result;
          },
          boost::asio::use_awaitable);
      }
      else
      {
        // Member function returns regular type
        using ResultType = RawResultType;
        using ReturnType = std::conditional_t<std::is_void_v<ResultType>, bool, std::optional<ResultType>>;

        return boost::asio::co_spawn(
          callingExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ReturnType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ReturnType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  if constexpr (std::is_void_v<ResultType>)
                  {
                    co_return false;
                  }
                  else
                  {
                    co_return std::nullopt;
                  }
                }

                if constexpr (std::is_void_v<ResultType>)
                {
                  func(ptr, std::move(args)...);
                  co_return true;
                }
                else
                {
                  co_return std::optional<ResultType>(func(ptr, std::move(args)...));
                }
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            co_return result;
          },
          boost::asio::use_awaitable);
      }
    }

    /// @brief Invokes a member function using a DispatchContext, returning optional on expiration.
    ///
    /// This helper uses a DispatchContext to automatically extract both source and target executors.
    /// After the operation completes on the target executor, execution resumes on the source executor.
    /// This is the non-throwing variant that returns std::nullopt or false on expiration.
    ///
    /// @tparam DebugHintName Optional debug hint (unused in non-throwing variant, kept for consistency).
    /// @tparam TSource Type of the source object in the DispatchContext.
    /// @tparam TTarget Type of the target object in the DispatchContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param dispatchContext The dispatch context containing source and target executor contexts.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable<std::optional<ResultType>> for non-void functions, or awaitable<bool> for void functions.
    ///         Returns std::nullopt or false if the target weak_ptr is expired.
    template <const char* DebugHintName = kEmptyDebugHint, typename TSource, typename TTarget, typename MemberFunc, typename... Args>
    auto TryInvokeAsync(const Lifecycle::DispatchContext<TSource, TTarget>& dispatchContext, MemberFunc memberFunc, Args&&... args)
    {
      return TryInvokeAsync<DebugHintName>(dispatchContext.GetSourceExecutor(), dispatchContext.GetTargetContext(), std::move(memberFunc),
                                           std::forward<Args>(args)...);
    }

    /// @brief Posts a member function invocation using an ExecutorContext.
    ///
    /// This helper is for fire-and-forget synchronous operations that don't need to await results.
    /// The weak_ptr validity check happens inside the posted lambda on the target executor.
    ///
    /// @tparam T Type of the object managed by the ExecutorContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param context The executor context containing the executor and weak_ptr.
    /// @param memberFunc Pointer to the member function to invoke.
    /// @param args Arguments to forward to the member function.
    /// @return true if the post operation succeeded, false if an exception occurred during post.
    template <typename T, typename MemberFunc, typename... Args>
    bool TryInvokePost(const Lifecycle::ExecutorContext<T>& context, MemberFunc memberFunc, Args&&... args) noexcept
    {
      auto executor = context.GetExecutor();
      auto weakPtr = context.GetWeakPtr();

      try
      {
        boost::asio::post(executor,
                          [weakPtr, func = std::mem_fn(memberFunc), ... args = std::forward<Args>(args)]() mutable
                          {
                            if (auto ptr = weakPtr.lock())
                            {
                              func(ptr, std::move(args)...);
                            }
                          });
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    // ========================================================================================================
    // DispatchContext-based API (dual-executor convenience wrappers)
    // ========================================================================================================

    /// @brief Invokes a member function using a DispatchContext, throwing on expiration.
    ///
    /// This is a convenience wrapper that extracts the source and target executors from
    /// the DispatchContext and delegates to the dual-executor InvokeAsync overload.
    /// The operation executes on the target executor and returns to the source executor.
    ///
    /// @tparam DebugHintName Optional debug hint for exception messages (compile-time const char*).
    /// @tparam TSource Type of the source object managed by the DispatchContext.
    /// @tparam TTarget Type of the target object managed by the DispatchContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param context The dispatch context containing source and target executor contexts.
    /// @param memberFunc Pointer to the member function to invoke on the target.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable that completes with the result of the member function invocation, resuming on source executor.
    /// @throws ServiceDisposedException if the target weak_ptr is expired.
    template <const char* DebugHintName = kEmptyDebugHint, typename TSource, typename TTarget, typename MemberFunc, typename... Args>
    auto InvokeAsync(const Lifecycle::DispatchContext<TSource, TTarget>& context, MemberFunc memberFunc, Args&&... args)
    {
      using RawResultType = std::invoke_result_t<MemberFunc, TTarget*, std::decay_t<Args>...>;
      auto sourceExecutor = context.GetSourceExecutor();
      auto targetExecutor = context.GetTargetExecutor();
      auto weakPtr = context.GetTargetContext().GetWeakPtr();

      // Check if the member function returns an awaitable
      if constexpr (Detail::is_awaitable_v<RawResultType>)
      {
        // Member function returns awaitable<U>, extract U
        using ResultType = Detail::awaitable_value_t<RawResultType>;

        return boost::asio::co_spawn(
          sourceExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ResultType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  throw ServiceDisposedException(DebugHintName);
                }

                co_return co_await func(ptr, std::move(args)...);
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            co_return result;
          },
          boost::asio::use_awaitable);
      }
      else
      {
        // Member function returns regular type
        using ResultType = RawResultType;

        return boost::asio::co_spawn(
          sourceExecutor,
          [targetExecutor, weakPtr, func = std::mem_fn(memberFunc),
           ... args = std::forward<Args>(args)]() mutable -> boost::asio::awaitable<ResultType>
          {
            // Execute on target thread
            auto result = co_await boost::asio::co_spawn(
              targetExecutor,
              [weakPtr, func = std::move(func), ... args = std::move(args)]() mutable -> boost::asio::awaitable<ResultType>
              {
                auto ptr = weakPtr.lock();
                if (!ptr)
                {
                  throw ServiceDisposedException(DebugHintName);
                }

                if constexpr (std::is_void_v<ResultType>)
                {
                  func(ptr, std::move(args)...);
                  co_return;
                }
                else
                {
                  co_return func(ptr, std::move(args)...);
                }
              },
              boost::asio::use_awaitable);

            // Result is now back on calling executor
            if constexpr (std::is_void_v<ResultType>)
            {
              co_return;
            }
            else
            {
              co_return result;
            }
          },
          boost::asio::use_awaitable);
      }
    }

    /// @brief Invokes a member function using a DispatchContext, returning optional on expiration.
    ///
    /// This is a convenience wrapper that extracts the source and target executors from
    /// the DispatchContext and delegates to the dual-executor TryInvokeAsync overload.
    /// The operation executes on the target executor and returns to the source executor.
    ///
    /// @tparam DebugHintName Optional debug hint (unused in non-throwing variant, kept for consistency).
    /// @tparam TSource Type of the source object managed by the DispatchContext.
    /// @tparam TTarget Type of the target object managed by the DispatchContext.
    /// @tparam MemberFunc Type of the member function pointer.
    /// @tparam Args Types of arguments to forward to the member function.
    /// @param context The dispatch context containing source and target executor contexts.
    /// @param memberFunc Pointer to the member function to invoke on the target.
    /// @param args Arguments to forward to the member function.
    /// @return awaitable<std::optional<ResultType>> for non-void functions, or awaitable<bool> for void functions.
    ///         Returns std::nullopt or false if the target weak_ptr is expired. Resumes on source executor.
    template <const char* DebugHintName = kEmptyDebugHint, typename TSource, typename TTarget, typename MemberFunc, typename... Args>
    auto TryInvokeAsync(const Lifecycle::DispatchContext<TSource, TTarget>& context, MemberFunc memberFunc, Args&&... args)
    {
      return TryInvokeAsync<DebugHintName>(context.GetSourceExecutor(), context.GetTargetContext(), memberFunc, std::forward<Args>(args)...);
    }

  }    // namespace Util
}    // namespace Test2

#endif
