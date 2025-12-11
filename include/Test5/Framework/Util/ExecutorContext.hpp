#ifndef SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_EXECUTORCONTEXT_HPP
#define SERVICE_FRAMEWORK_TEST5_FRAMEWORK_UTIL_EXECUTORCONTEXT_HPP
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

#include <boost/asio.hpp>
#include <memory>
#include <utility>

namespace Test5
{
  /// @brief Provides safe access to a boost::asio io_context with automatic lifetime management.
  ///
  /// ExecutorContext holds a shared_ptr to an io_context, ensuring the context remains alive
  /// as long as any ExecutorContext (or copy) exists. This provides automatic lifetime safety
  /// for async operations that may still be running during shutdown.
  ///
  /// Typical usage pattern:
  /// - io_context created as shared_ptr by framework
  /// - ExecutorContext instances created per-service with shared ownership
  /// - io_context remains valid until all ExecutorContext copies are destroyed
  /// - Framework can safely destroy its io_context owner, refcount handles cleanup
  ///
  /// Usage:
  /// @code
  /// // Framework creates io_context as shared_ptr
  /// auto ioContext = std::make_shared<boost::asio::io_context>();
  ///
  /// class MyService {
  ///     ExecutorContext m_context;
  ///
  /// public:
  ///     MyService(std::shared_ptr<boost::asio::io_context> ioContext)
  ///         : m_context(ioContext)
  ///     {}
  ///
  ///     void DoWork() {
  ///         m_context.Post([]() {
  ///             // Work executes on io_context
  ///         });
  ///     }
  /// };
  /// @endcode
  class ExecutorContext
  {
    std::shared_ptr<boost::asio::io_context> m_ioContext;

  public:
    /// @brief Constructs an ExecutorContext with guaranteed io_context lifetime.
    ///
    /// @param ioContext Shared pointer to the io_context for lifetime management.
    explicit ExecutorContext(std::shared_ptr<boost::asio::io_context> ioContext)
      : m_ioContext(std::move(ioContext))
    {
    }

    /// @brief Gets the associated executor.
    /// @return The executor from the underlying io_context.
    boost::asio::any_io_executor GetExecutor() const
    {
      return m_ioContext->get_executor();
    }

    /// @brief Posts work to the executor.
    ///
    /// The handler will be queued for execution on the io_context.
    ///
    /// @tparam Handler Type of the handler callable.
    /// @param handler The handler to execute on the executor.
    template <typename Handler>
    void Post(Handler&& handler) const
    {
      boost::asio::post(m_ioContext->get_executor(), std::forward<Handler>(handler));
    }
  };
}    // namespace Test5

#endif
