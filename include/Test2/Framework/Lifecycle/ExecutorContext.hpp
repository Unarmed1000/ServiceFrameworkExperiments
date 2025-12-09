#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_EXECUTORCONTEXT_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_EXECUTORCONTEXT_HPP
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
#include <memory>
#include <utility>

namespace Test2::Lifecycle
{
  /// @brief Lifetime-aware executor context that pairs an executor with a weak_ptr for lifetime tracking.
  ///
  /// This class encapsulates both the executor and the weak_ptr to the target object, ensuring
  /// that lifetime tracking is always paired with the executor when making cross-thread calls.
  ///
  /// @tparam T The type of the object whose lifetime is being tracked.
  template <typename T>
  class ExecutorContext
  {
    boost::asio::any_io_executor m_executor;
    std::weak_ptr<T> m_weakPtr;

  public:
    /// @brief Constructs an executor context from a shared_ptr and executor.
    /// @param ptr Shared pointer to the target object.
    /// @param executor The executor associated with the target object's thread.
    ExecutorContext(std::shared_ptr<T> ptr, boost::asio::any_io_executor executor)
      : m_executor(std::move(executor))
      , m_weakPtr(std::move(ptr))
    {
    }

    /// @brief Gets the executor.
    [[nodiscard]] const boost::asio::any_io_executor& GetExecutor() const noexcept
    {
      return m_executor;
    }

    /// @brief Gets the weak pointer.
    [[nodiscard]] const std::weak_ptr<T>& GetWeakPtr() const noexcept
    {
      return m_weakPtr;
    }

    /// @brief Attempts to lock the weak pointer.
    [[nodiscard]] std::shared_ptr<T> TryLock() const noexcept
    {
      return m_weakPtr.lock();
    }

    /// @brief Checks if the tracked object is still alive.
    [[nodiscard]] bool IsAlive() const noexcept
    {
      return !m_weakPtr.expired();
    }
  };
}    // namespace Test2::Lifecycle

#endif
