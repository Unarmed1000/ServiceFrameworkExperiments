#ifndef SERVICE_FRAMEWORK_TEST3_QTSLOT_EXAMPLESERVICEUSE_HPP
#define SERVICE_FRAMEWORK_TEST3_QTSLOT_EXAMPLESERVICEUSE_HPP
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

#include <boost/asio/awaitable.hpp>
#include <QMetaObject>
#include <future>
#include <stdexcept>
#include "QtSlotCallbackReceiver.hpp"

namespace Test3
{
  namespace QtSlot
  {
    /// @brief Example class demonstrating usage of QtSlotCallbackReceiver and ToFutureWithCallback.
    ///
    /// This class shows how to:
    /// - Inherit from QtSlotCallbackReceiver for convenient async operations
    /// - Use Q_OBJECT macro and declare callback slots
    /// - Handle both simple and connection-tracked async operations
    /// - Handle results, errors, void operations, and cancellation
    /// - Manage multiple concurrent operations
    ///
    /// All callback slots are automatically invoked on the Qt thread associated with
    /// this QObject, ensuring thread-safe UI updates and state management.
    class ExampleServiceUse : public QtSlotCallbackReceiver
    {
      Q_OBJECT

    public:
      /// @brief Constructs an example service user.
      /// @param executor The executor to run async operations on.
      /// @param parent Optional parent QObject.
      explicit ExampleServiceUse(boost::asio::any_io_executor executor, QObject* parent = nullptr)
        : QtSlotCallbackReceiver(std::move(executor), parent)
        , m_operationCount(0)
      {
      }

      // ============================================================================
      // Example 1: Simple Usage - Fire and Forget
      // ============================================================================

      /// @brief Demonstrates simple async call without tracking the connection.
      ///
      /// The callback slot is invoked when the operation completes. The future
      /// is returned but can be ignored if you only care about the callback.
      void DoSimpleWork()
      {
        // Simple async call - just provide callback slot and lambda
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                                []() -> boost::asio::awaitable<double>
                                {
                                  // Simulate async work
                                  co_return 42.0;
                                });

        // Future can be stored or ignored - callback will still be invoked
      }

      // ============================================================================
      // Example 2: Connection Tracking for Cancellation
      // ============================================================================

      /// @brief Demonstrates connection-tracked async call with explicit disconnect.
      ///
      /// Stores the connection handle to allow explicit disconnection before
      /// the operation completes, useful for cancellation scenarios.
      void DoTrackableWork()
      {
        // Connection-tracked async call - returns both future and connection
        auto result = CallAsyncTracked(&ExampleServiceUse::HandleDoubleResult,
                                       []() -> boost::asio::awaitable<double>
                                       {
                                         // Simulate longer async work
                                         co_return 123.45;
                                       });

        // Store connection for later cancellation
        m_activeConnection = result.connection;
        m_activeFuture = std::move(result.future);
      }

      /// @brief Cancels the active trackable operation.
      ///
      /// Disconnects the callback so it won't be invoked when the operation completes.
      /// The async work still runs to completion, but the callback is skipped.
      void CancelTrackableWork()
      {
        if (m_activeConnection)
        {
          QObject::disconnect(m_activeConnection);
          m_activeConnection = QMetaObject::Connection();
        }
      }

      // ============================================================================
      // Example 3: Error Handling
      // ============================================================================

      /// @brief Demonstrates error handling with exceptions.
      ///
      /// Shows how exceptions thrown in the coroutine are captured in the future
      /// and can be handled in the callback slot.
      void DoWorkThatMightFail(bool shouldFail)
      {
        CallAsync(&ExampleServiceUse::HandleDoubleResult,
                  [shouldFail]() -> boost::asio::awaitable<double>
                  {
                    if (shouldFail)
                    {
                      throw std::runtime_error("Simulated async operation failure");
                    }
                    co_return 99.99;
                  });
      }

      // ============================================================================
      // Example 4: Void Operations
      // ============================================================================

      /// @brief Demonstrates async operations that return void.
      ///
      /// Shows that the pattern works equally well for operations that don't
      /// return a value, only signaling completion.
      void DoVoidWork()
      {
        CallAsync(&ExampleServiceUse::HandleVoidResult,
                  []() -> boost::asio::awaitable<void>
                  {
                    // Simulate async work with no return value
                    co_return;
                  });
      }

      // ============================================================================
      // Example 5: Multiple Concurrent Operations
      // ============================================================================

      /// @brief Demonstrates launching multiple concurrent async operations.
      ///
      /// Shows that multiple operations can run concurrently, each with their
      /// own callback invocation.
      void DoMultipleOperations()
      {
        m_operationCount = 0;

        // Launch multiple operations concurrently
        for (int i = 0; i < 5; ++i)
        {
          CallAsync(&ExampleServiceUse::HandleConcurrentResult,
                    [i]() -> boost::asio::awaitable<int>
                    {
                      // Each operation returns its index
                      co_return i;
                    });
        }
      }

      // ============================================================================
      // Example 6: Using Future for Synchronous Wait
      // ============================================================================

      /// @brief Demonstrates using the returned future for synchronous waiting.
      ///
      /// Shows that while callbacks provide async notification, the future can
      /// also be used for synchronous result retrieval if needed.
      double DoSynchronousWork()
      {
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult, []() -> boost::asio::awaitable<double> { co_return 777.0; });

        // Block and wait for result (callback will also be invoked)
        return future.get();
      }

    private slots:
      /// @brief Callback slot invoked when double result is ready.
      /// @param future Future containing the result or exception.
      void HandleDoubleResult(std::future<double> future)
      {
        try
        {
          double result = future.get();
          // Process result (e.g., update UI, log, emit signals)
          // qDebug() << "Result:" << result;
        }
        catch (const std::exception& ex)
        {
          // Handle error (e.g., show error message, log, emit error signal)
          // qWarning() << "Error:" << ex.what();
        }
      }

      /// @brief Callback slot invoked when void operation completes.
      /// @param future Future signaling completion or exception.
      void HandleVoidResult(std::future<void> future)
      {
        try
        {
          future.get();    // Check for exceptions
          // Handle successful completion
          // qDebug() << "Void operation completed";
        }
        catch (const std::exception& ex)
        {
          // Handle error
          // qWarning() << "Error:" << ex.what();
        }
      }

      /// @brief Callback slot invoked for concurrent operations.
      /// @param future Future containing the operation index.
      void HandleConcurrentResult(std::future<int> future)
      {
        try
        {
          int index = future.get();
          ++m_operationCount;
          // Process concurrent result
          // qDebug() << "Concurrent operation" << index << "completed. Total:" << m_operationCount;

          // Check if all operations completed
          if (m_operationCount == 5)
          {
            // All concurrent operations finished
            // emit allOperationsCompleted();
          }
        }
        catch (const std::exception& ex)
        {
          // Handle error
          // qWarning() << "Concurrent operation error:" << ex.what();
        }
      }

    private:
      QMetaObject::Connection m_activeConnection;
      std::future<double> m_activeFuture;
      int m_operationCount;
    };
  }    // namespace QtSlot
}    // namespace Test3

#endif
