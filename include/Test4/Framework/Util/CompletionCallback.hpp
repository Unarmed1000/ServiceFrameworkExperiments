#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_COMPLETIONCALLBACK_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_UTIL_COMPLETIONCALLBACK_HPP
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

#include <future>
#include <memory>
#include <utility>

namespace Test4
{
  /// @brief Type-erased, moveable completion callback wrapper.
  ///
  /// Completely hides callback implementation details (method pointers, QPointer,
  /// Qt slots, lifetime tracking, executor marshaling) behind a moveable value type.
  /// Use FutureCallbackHelper to create instances.
  template <typename TResult>
  class CompletionCallback final
  {
  public:
    /// @brief Internal interface for callback implementations.
    class ICallbackImpl
    {
    public:
      virtual ~ICallbackImpl() = default;
      virtual void Invoke(std::future<TResult> result) = 0;
    };

  private:
    std::unique_ptr<ICallbackImpl> m_impl;

  public:
    /// @brief Creates an empty callback (no-op when invoked).
    CompletionCallback() = default;

    /// @brief Internal constructor for callback implementations.
    /// @note Use FutureCallbackHelper to create callbacks, not this constructor directly.
    template <typename TImpl>
    explicit CompletionCallback(std::unique_ptr<TImpl> impl)
      : m_impl(std::move(impl))
    {
    }

    CompletionCallback(CompletionCallback&&) = default;
    CompletionCallback& operator=(CompletionCallback&&) = default;

    CompletionCallback(const CompletionCallback&) = delete;
    CompletionCallback& operator=(const CompletionCallback&) = delete;

    /// @brief Checks if this callback has an implementation.
    /// @return true if callback will do something when invoked, false if empty.
    explicit operator bool() const noexcept
    {
      return m_impl != nullptr;
    }

    /// @brief Invokes the callback with the operation result.
    /// @param result The future containing the operation result.
    /// @note No-op if callback is empty.
    void Invoke(std::future<TResult> result)
    {
      if (m_impl)
      {
        m_impl->Invoke(std::move(result));
      }
    }
  };
}

#endif
