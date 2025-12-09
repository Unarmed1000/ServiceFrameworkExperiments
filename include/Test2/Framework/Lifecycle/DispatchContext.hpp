#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_DISPATCHCONTEXT_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_DISPATCHCONTEXT_HPP
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

#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <utility>

namespace Test2
{
  /// @brief Context for dispatching cross-thread invocations between a source and target executor.
  ///
  /// This class combines two ExecutorContext objects: one for the source (calling) context and
  /// one for the target context. It's designed for scenarios where you need to invoke operations
  /// on a target object running on a different thread, and have the result returned to the
  /// source thread.
  ///
  /// @tparam TSource The type of the source object (often a lifetime tracker).
  /// @tparam TTarget The type of the target object to invoke methods on.
  template <typename TSource, typename TTarget>
  class DispatchContext
  {
    ExecutorContext<TSource> m_sourceContext;
    ExecutorContext<TTarget> m_targetContext;

  public:
    /// @brief Constructs a dispatch context from source and target executor contexts.
    /// @param sourceContext The executor context for the source (calling) thread.
    /// @param targetContext The executor context for the target object to invoke methods on.
    DispatchContext(ExecutorContext<TSource> sourceContext, ExecutorContext<TTarget> targetContext)
      : m_sourceContext(std::move(sourceContext))
      , m_targetContext(std::move(targetContext))
    {
    }

    /// @brief Gets the source executor context.
    [[nodiscard]] const ExecutorContext<TSource>& GetSourceContext() const noexcept
    {
      return m_sourceContext;
    }

    /// @brief Gets the target executor context.
    [[nodiscard]] const ExecutorContext<TTarget>& GetTargetContext() const noexcept
    {
      return m_targetContext;
    }

    /// @brief Gets the source executor.
    [[nodiscard]] boost::asio::any_io_executor GetSourceExecutor() const noexcept
    {
      return m_sourceContext.GetExecutor();
    }

    /// @brief Gets the target executor.
    [[nodiscard]] boost::asio::any_io_executor GetTargetExecutor() const noexcept
    {
      return m_targetContext.GetExecutor();
    }

    /// @brief Checks if the source object is still alive.
    [[nodiscard]] bool IsSourceAlive() const noexcept
    {
      return m_sourceContext.IsAlive();
    }

    /// @brief Checks if the target object is still alive.
    [[nodiscard]] bool IsTargetAlive() const noexcept
    {
      return m_targetContext.IsAlive();
    }
  };
}    // namespace Test2

#endif
