#ifndef SERVICE_FRAMEWORK_TEST3_QTPOINTER_EXAMPLESERVICEUSE_HPP
#define SERVICE_FRAMEWORK_TEST3_QTPOINTER_EXAMPLESERVICEUSE_HPP
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

#include "QtPointerCallbackReceiver.hpp"
#include <boost/asio/awaitable.hpp>
#include <future>
#include <stdexcept>

namespace Test3
{
  namespace QtPointer
  {
    /// @brief Example class demonstrating usage of QtPointerCallbackReceiver and ToFutureWithCallback.
    ///
    /// This class shows how to:
    /// - Inherit from QtPointerCallbackReceiver without Q_OBJECT macro
    /// - Use regular member functions as callbacks (no slots required)
    /// - Handle results, errors, void operations
    /// - Benefit from automatic lifetime safety via QPointer
    /// - Manage multiple concurrent operations
    ///
    /// All callback methods are automatically invoked on the Qt thread associated with
    /// this QObject, ensuring thread-safe UI updates and state management.
    ///
    /// NOTE: No Q_OBJECT macro needed - this class can be used without MOC processing.
    class ExampleServiceUse : public QtPointerCallbackReceiver
    {
    public:
      /// @brief Constructs an example service user.
      /// @param executor The executor to run async operations on.
      /// @param parent Optional parent QObject.
      explicit ExampleServiceUse(boost::asio::any_io_executor executor, QObject* parent = nullptr)
        : QtPointerCallbackReceiver(std::move(executor), parent)
        , m_operationCount(0)
      {
      }

      // ============================================================================
      // Example 1: Simple Usage - Fire and Forget
      // ============================================================================

      /// @brief Demonstrates simple async call without tracking.
      ///
      /// The callback method is invoked when the operation completes. The future
      /// is returned but can be ignored if you only care about the callback.
      /// If this object is destroyed before completion, the callback is safely skipped.
      void DoSimpleWork()
      {
        // Simple async call - just provide callback method and lambda
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                                []() -> boost::asio::awaitable<double>
                                {
                                  // Simulate async work
                                  co_return 42.0;
                                });

        // Future can be stored or ignored - callback will still be invoked
        // (unless object is destroyed before completion)
      }

      // ============================================================================
      // Example 2: Error Handling
      // ============================================================================

      /// @brief Demonstrates error handling with exceptions.
      ///
      /// Shows how exceptions thrown in the coroutine are captured in the future
      /// and can be handled in the callback method.
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
      // Example 3: Void Operations
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
      // Example 4: Multiple Concurrent Operations
      // ============================================================================

      /// @brief Demonstrates launching multiple concurrent async operations.
      ///
      /// Shows that multiple operations can run concurrently, each with their
      /// own callback invocation. All callbacks are safely skipped if this
      /// object is destroyed while operations are in flight.
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
      // Example 5: Using Future for Synchronous Wait
      // ============================================================================

      /// @brief Demonstrates using the returned future for synchronous waiting.
      ///
      /// Shows that while callbacks provide async notification, the future can
      /// also be used for synchronous result retrieval if needed.
      double DoSynchronousWork()
      {
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                                []() -> boost::asio::awaitable<double>
                                {
                                  co_return 777.0;
                                });

        // Block and wait for result (callback will also be invoked if object still alive)
        return future.get();
      }

      // ============================================================================
      // Example 6: Automatic Cleanup on Destruction
      // ============================================================================

      /// @brief Demonstrates automatic safety when object is destroyed.
      ///
      /// If this object is deleted while async operations are in flight,
      /// the QPointer becomes null and callbacks are safely skipped.
      /// This prevents use-after-free without explicit cancellation.
      void DoLongRunningWork()
      {
        CallAsync(&ExampleServiceUse::HandleDoubleResult,
                  []() -> boost::asio::awaitable<double>
                  {
                    // Simulate long-running operation
                    // If ExampleServiceUse is deleted during this time,
                    // HandleDoubleResult will NOT be invoked
                    co_return 888.0;
                  });

        // Safe to delete this object immediately after calling DoLongRunningWork()
        // The callback will be automatically skipped via QPointer null check
      }

      // ============================================================================
      // Example 7: Exception Handling with Different Error Types
      // ============================================================================

      /// @brief Demonstrates handling multiple exception types.
      ///
      /// Shows how different exception types can be caught and handled
      /// separately in the callback.
      void DoWorkWithCustomErrors(int errorType)
      {
        CallAsync(&ExampleServiceUse::HandleResultWithCustomErrors,
                  [errorType]() -> boost::asio::awaitable<std::string>
                  {
                    if (errorType == 1)
                    {
                      throw std::invalid_argument("Invalid input parameter");
                    }
                    else if (errorType == 2)
                    {
                      throw std::runtime_error("Runtime processing error");
                    }
                    co_return std::string("Success");
                  });
      }

      // ============================================================================
      // Example 8: Chaining Operations
      // ============================================================================

      /// @brief Demonstrates storing futures for later chaining.
      ///
      /// Shows how futures can be stored and used to coordinate
      /// multiple async operations.
      void DoChainedWork()
      {
        // First operation
        m_firstFuture = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                                  []() -> boost::asio::awaitable<double>
                                  {
                                    co_return 10.0;
                                  });

        // Second operation (in real code, might depend on first)
        m_secondFuture = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                                   []() -> boost::asio::awaitable<double>
                                   {
                                     co_return 20.0;
                                   });

        // Could later check if both completed with m_firstFuture.get() + m_secondFuture.get()
      }

    private:
      // ============================================================================
      // Callback Methods (Regular Member Functions - No Q_OBJECT or slots needed)
      // ============================================================================

      /// @brief Callback method invoked when double result is ready.
      ///
      /// NOTE: This is a regular member function, not a Qt slot.
      /// No Q_OBJECT macro or MOC processing required.
      ///
      /// @param future Future containing the result or exception.
      void HandleDoubleResult(std::future<double> future)
      {
        try
        {
          double result = future.get();
          // Process result (e.g., update UI, log, emit signals if using Q_OBJECT in subclass)
          // qDebug() << "Result:" << result;
        }
        catch (const std::exception& ex)
        {
          // Handle error (e.g., show error message, log)
          // qWarning() << "Error:" << ex.what();
        }
      }

      /// @brief Callback method invoked when void operation completes.
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

      /// @brief Callback method invoked for concurrent operations.
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
            // Could emit signal if using Q_OBJECT: emit allOperationsCompleted();
          }
        }
        catch (const std::exception& ex)
        {
          // Handle error
          // qWarning() << "Concurrent operation error:" << ex.what();
        }
      }

      /// @brief Callback method demonstrating custom error handling.
      /// @param future Future containing the result or various exception types.
      void HandleResultWithCustomErrors(std::future<std::string> future)
      {
        try
        {
          std::string result = future.get();
          // Process successful result
          // qDebug() << "Success:" << QString::fromStdString(result);
        }
        catch (const std::invalid_argument& ex)
        {
          // Handle validation errors specifically
          // qWarning() << "Validation error:" << ex.what();
        }
        catch (const std::runtime_error& ex)
        {
          // Handle runtime errors specifically
          // qWarning() << "Runtime error:" << ex.what();
        }
        catch (const std::exception& ex)
        {
          // Handle any other errors
          // qWarning() << "Unexpected error:" << ex.what();
        }
      }

      // Member variables
      int m_operationCount;
      std::future<double> m_firstFuture;
      std::future<double> m_secondFuture;
    };
  }    // namespace QtPointer
}    // namespace Test3

#endif
