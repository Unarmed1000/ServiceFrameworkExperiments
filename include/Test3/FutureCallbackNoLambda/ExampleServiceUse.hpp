#ifndef SERVICE_FRAMEWORK_TEST3_NOLAMBDA_EXAMPLESERVICEUSE_HPP
#define SERVICE_FRAMEWORK_TEST3_NOLAMBDA_EXAMPLESERVICEUSE_HPP
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
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "ServiceCallbackReceiver.hpp"

namespace Test3
{
  namespace NoLambda
  {
    // ============================================================================
    // Mock Service Interfaces (for demonstration)
    // ============================================================================

    /// @brief Mock calculator service with async operations
    class ICalculatorService
    {
    public:
      virtual ~ICalculatorService() = default;

      /// @brief Async addition operation
      virtual boost::asio::awaitable<double> AddAsync(double a, double b) = 0;

      /// @brief Async operation that might fail
      virtual boost::asio::awaitable<double> DivideAsync(double a, double b) = 0;

      /// @brief Async void operation
      virtual boost::asio::awaitable<void> LogOperationAsync(const std::string& message) = 0;
    };

    /// @brief Mock implementation of calculator service
    class MockCalculatorService : public ICalculatorService
    {
    public:
      boost::asio::awaitable<double> AddAsync(double a, double b) override
      {
        // Simulate async work
        co_return a + b;
      }

      boost::asio::awaitable<double> DivideAsync(double a, double b) override
      {
        if (b == 0.0)
        {
          throw std::invalid_argument("Division by zero");
        }
        co_return a / b;
      }

      boost::asio::awaitable<void> LogOperationAsync(const std::string& message) override
      {
        std::cout << "[LOG] " << message << std::endl;
        co_return;
      }
    };

    // ============================================================================
    // Free Functions (for demonstration)
    // ============================================================================

    /// @brief Static async function
    inline boost::asio::awaitable<std::string> FormatMessageAsync(const std::string& name, int id)
    {
      co_return name + " (ID: " + std::to_string(id) + ")";
    }

    /// @brief Static async function with multiple parameters
    inline boost::asio::awaitable<int> CalculateSumAsync(int a, int b, int c)
    {
      co_return a + b + c;
    }

    // ============================================================================
    // Example Usage Class
    // ============================================================================

    /// @brief Example class demonstrating usage of ServiceCallbackReceiver without lambda wrappers.
    ///
    /// This class shows how to:
    /// - Inherit from ServiceCallbackReceiver for automatic stop_token management
    /// - Call awaitable-returning functions directly without lambda wrappers
    /// - Implement callback methods that receive std::future<T>
    /// - Handle errors, void operations, and member functions
    ///
    /// The pattern ensures that callbacks are only invoked if the object is still alive,
    /// preventing use-after-free scenarios in async code.
    class ExampleServiceUse : public ServiceCallbackReceiver
    {
    public:
      /// @brief Constructs an example service user.
      /// @param executor The executor to run callbacks on.
      /// @param calcService The calculator service to use.
      explicit ExampleServiceUse(boost::asio::any_io_executor executor, ICalculatorService* calcService)
        : ServiceCallbackReceiver(std::move(executor))
        , m_calcService(calcService)
      {
      }

      // ============================================================================
      // Example 1: Direct Member Function Call (No Lambda Wrapper!)
      // ============================================================================

      /// @brief Demonstrates calling a service method directly without lambda wrapper.
      ///
      /// OLD WAY (with lambda wrapper):
      ///   CallAsync(&ExampleServiceUse::HandleDoubleResult,
      ///             [this]() { return m_calcService->AddAsync(10.0, 32.0); });
      ///
      /// NEW WAY (direct call):
      ///   CallAsync(&ExampleServiceUse::HandleDoubleResult,
      ///             &ICalculatorService::AddAsync, m_calcService, 10.0, 32.0);
      void DoSimpleCalculation()
      {
        // Direct call - no lambda needed!
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult, &ICalculatorService::AddAsync, m_calcService, 10.0, 32.0);

        // Future can be stored or ignored - callback will still be invoked
      }

      // ============================================================================
      // Example 2: Error Handling
      // ============================================================================

      /// @brief Demonstrates error handling with direct function calls.
      void DoDivision(double numerator, double denominator)
      {
        // Direct call to function that might throw
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult, &ICalculatorService::DivideAsync, m_calcService, numerator, denominator);
      }

      // ============================================================================
      // Example 3: Void Operations
      // ============================================================================

      /// @brief Demonstrates calling void-returning awaitable functions.
      void DoLogging(const std::string& message)
      {
        auto future = CallAsync(&ExampleServiceUse::HandleVoidResult, &ICalculatorService::LogOperationAsync, m_calcService, message);
      }

      // ============================================================================
      // Example 4: Free Functions
      // ============================================================================

      /// @brief Demonstrates calling free awaitable-returning functions.
      void DoFormatMessage(const std::string& name, int id)
      {
        // Call free function directly
        auto future = CallAsync(&ExampleServiceUse::HandleStringResult, &FormatMessageAsync, name, id);
      }

      // ============================================================================
      // Example 5: Multiple Arguments
      // ============================================================================

      /// @brief Demonstrates calling functions with multiple arguments.
      void DoMultiArgCalculation(int a, int b, int c)
      {
        auto future = CallAsync(&ExampleServiceUse::HandleIntResult, &CalculateSumAsync, a, b, c);
      }

      // ============================================================================
      // Example 6: Batch Operations
      // ============================================================================

      /// @brief Demonstrates launching multiple concurrent operations.
      void DoBatchCalculations()
      {
        m_operationCount = 0;
        m_expectedCount = 3;

        // Launch multiple operations concurrently
        CallAsync(&ExampleServiceUse::HandleBatchResult, &ICalculatorService::AddAsync, m_calcService, 1.0, 2.0);
        CallAsync(&ExampleServiceUse::HandleBatchResult, &ICalculatorService::AddAsync, m_calcService, 5.0, 10.0);
        CallAsync(&ExampleServiceUse::HandleBatchResult, &ICalculatorService::AddAsync, m_calcService, 20.0, 30.0);
      }

      // ============================================================================
      // Example 7: Member Function of This Class
      // ============================================================================

      /// @brief Demonstrates calling a member function of this class.
      void DoLocalCalculation()
      {
        // Call member function of this class
        auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult, &ExampleServiceUse::LocalAsyncOperation, this, 42.0);
      }

      // ============================================================================
      // Example 8: Comparison - Lambda vs Direct Call
      // ============================================================================

      /// @brief Shows the difference between lambda wrapper and direct call side by side.
      void CompareApproaches()
      {
        // OLD APPROACH: Lambda wrapper (verbose)
        // CallAsync(&ExampleServiceUse::HandleDoubleResult,
        //           [this]() { return m_calcService->AddAsync(100.0, 200.0); });

        // NEW APPROACH: Direct call (concise)
        CallAsync(&ExampleServiceUse::HandleDoubleResult, &ICalculatorService::AddAsync, m_calcService, 100.0, 200.0);
      }

    private:
      // ============================================================================
      // Async Operation (for demonstration of calling member functions)
      // ============================================================================

      /// @brief A local async operation that returns an awaitable.
      boost::asio::awaitable<double> LocalAsyncOperation(double input)
      {
        // Simulate async work
        co_return input * 2.0;
      }

      // ============================================================================
      // Callback Methods
      // ============================================================================

      /// @brief Callback invoked when double result is ready.
      /// @param future Future containing the result or exception.
      void HandleDoubleResult(std::future<double> future)
      {
        try
        {
          double result = future.get();
          std::cout << "Result: " << result << std::endl;
        }
        catch (const std::exception& ex)
        {
          std::cerr << "Error: " << ex.what() << std::endl;
        }
      }

      /// @brief Callback invoked when void operation completes.
      /// @param future Future signaling completion or exception.
      void HandleVoidResult(std::future<void> future)
      {
        try
        {
          future.get();    // Check for exceptions
          std::cout << "Void operation completed" << std::endl;
        }
        catch (const std::exception& ex)
        {
          std::cerr << "Error: " << ex.what() << std::endl;
        }
      }

      /// @brief Callback invoked when string result is ready.
      /// @param future Future containing the string result.
      void HandleStringResult(std::future<std::string> future)
      {
        try
        {
          std::string result = future.get();
          std::cout << "String result: " << result << std::endl;
        }
        catch (const std::exception& ex)
        {
          std::cerr << "Error: " << ex.what() << std::endl;
        }
      }

      /// @brief Callback invoked when int result is ready.
      /// @param future Future containing the int result.
      void HandleIntResult(std::future<int> future)
      {
        try
        {
          int result = future.get();
          std::cout << "Int result: " << result << std::endl;
        }
        catch (const std::exception& ex)
        {
          std::cerr << "Error: " << ex.what() << std::endl;
        }
      }

      /// @brief Callback invoked for batch operations.
      /// @param future Future containing the operation result.
      void HandleBatchResult(std::future<double> future)
      {
        try
        {
          double result = future.get();
          ++m_operationCount;
          std::cout << "Batch result: " << result << " (completed: " << m_operationCount << "/" << m_expectedCount << ")" << std::endl;

          if (m_operationCount == m_expectedCount)
          {
            std::cout << "All batch operations complete!" << std::endl;
          }
        }
        catch (const std::exception& ex)
        {
          std::cerr << "Batch error: " << ex.what() << std::endl;
        }
      }

      // Member variables
      ICalculatorService* m_calcService;
      int m_operationCount = 0;
      int m_expectedCount = 0;
    };

    // ============================================================================
    // Main Example Function
    // ============================================================================

    /// @brief Demonstrates all usage patterns.
    inline void RunExamples()
    {
      boost::asio::io_context ioc;
      MockCalculatorService calcService;

      auto example = std::make_shared<ExampleServiceUse>(ioc.get_executor(), &calcService);

      std::cout << "=== Example 1: Simple Calculation ===" << std::endl;
      example->DoSimpleCalculation();

      std::cout << "\n=== Example 2: Division (Success) ===" << std::endl;
      example->DoDivision(10.0, 2.0);

      std::cout << "\n=== Example 3: Division (Error) ===" << std::endl;
      example->DoDivision(10.0, 0.0);

      std::cout << "\n=== Example 4: Void Operation ===" << std::endl;
      example->DoLogging("Test message");

      std::cout << "\n=== Example 5: Free Function ===" << std::endl;
      example->DoFormatMessage("User", 123);

      std::cout << "\n=== Example 6: Multiple Arguments ===" << std::endl;
      example->DoMultiArgCalculation(1, 2, 3);

      std::cout << "\n=== Example 7: Batch Operations ===" << std::endl;
      example->DoBatchCalculations();

      std::cout << "\n=== Example 8: Member Function ===" << std::endl;
      example->DoLocalCalculation();

      // Run the event loop
      ioc.run();
    }
  }    // namespace NoLambda
}    // namespace Test3

#endif
