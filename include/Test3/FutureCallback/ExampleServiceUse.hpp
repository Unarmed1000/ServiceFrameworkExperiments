#ifndef SERVICE_FRAMEWORK_TEST3_EXAMPLESERVICEUSE_HPP
#define SERVICE_FRAMEWORK_TEST3_EXAMPLESERVICEUSE_HPP
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

#include <Test3/ServiceCallbackReceiver.hpp>
#include <Test3/ToFutureWithCallback.hpp>
#include <boost/asio/io_context.hpp>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Test3
{
  // ============================================================================
  // Mock Service Interface (for demonstration)
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
  // Example Usage Class
  // ============================================================================

  /// @brief Example class demonstrating usage of ServiceCallbackReceiver and ToFutureWithCallback.
  ///
  /// This class shows how to:
  /// - Inherit from ServiceCallbackReceiver for automatic stop_token management
  /// - Use CallAsync helper for clean async operation invocation
  /// - Implement callback methods that receive std::future<T>
  /// - Handle errors, void operations, and batch processing
  /// - Chain multiple async operations using co_await
  ///
  /// The pattern ensures that callbacks are only invoked if the object is still alive,
  /// preventing use-after-free scenarios in async code.
  class ExampleServiceUse : public ServiceCallbackReceiver
  {
  public:
    /// @brief Constructs an example service user.
    /// @param executor The executor to run callbacks on.
    /// @param calcService The calculator service to use (optional).
    explicit ExampleServiceUse(boost::asio::any_io_executor executor, ICalculatorService* calcService = nullptr)
      : ServiceCallbackReceiver(std::move(executor))
      , m_calcService(calcService)
    {
    }

    // ============================================================================
    // Example 1: Simple Lambda Coroutine
    // ============================================================================

    /// @brief Demonstrates basic lambda coroutine usage.
    void DoSimpleWork()
    {
      auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                              []() -> boost::asio::awaitable<double>
                              {
                                // Inline async logic
                                co_return 42.0;
                              });
    }

    // ============================================================================
    // Example 2: Calling Service Method in Lambda
    // ============================================================================

    /// @brief Demonstrates wrapping a service call in a lambda.
    void DoServiceCall()
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                              [this]() -> boost::asio::awaitable<double>
                              {
                                // Call service and return its awaitable
                                return m_calcService->AddAsync(10.0, 32.0);
                              });
    }

    // ============================================================================
    // Example 3: Error Handling
    // ============================================================================

    /// @brief Demonstrates error handling in lambda coroutines.
    void DoWorkThatMightFail(bool shouldFail)
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                              [this, shouldFail]() -> boost::asio::awaitable<double>
                              {
                                if (shouldFail)
                                {
                                  return m_calcService->DivideAsync(10.0, 0.0);
                                }
                                return m_calcService->AddAsync(10.0, 5.0);
                              });
    }

    // ============================================================================
    // Example 4: Void Operations
    // ============================================================================

    /// @brief Demonstrates void-returning awaitable operations.
    void DoVoidWork(const std::string& message)
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleVoidResult,
                              [this, message]() -> boost::asio::awaitable<void> { return m_calcService->LogOperationAsync(message); });
    }

    // ============================================================================
    // Example 5: Chaining Multiple Async Calls
    // ============================================================================

    /// @brief Demonstrates chaining multiple awaitable calls in a lambda.
    void DoChainedWork()
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                              [this]() -> boost::asio::awaitable<double>
                              {
                                // Chain multiple async operations
                                auto result1 = co_await m_calcService->AddAsync(10.0, 20.0);
                                auto result2 = co_await m_calcService->AddAsync(result1, 5.0);
                                co_return result2;
                              });
    }

    // ============================================================================
    // Example 6: Complex Logic with Conditionals
    // ============================================================================

    /// @brief Demonstrates complex inline coroutine logic.
    void DoComplexWork(double input)
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleDoubleResult,
                              [this, input]() -> boost::asio::awaitable<double>
                              {
                                // Complex logic with branching
                                if (input > 100.0)
                                {
                                  auto temp = co_await m_calcService->AddAsync(input, 50.0);
                                  co_return co_await m_calcService->DivideAsync(temp, 2.0);
                                }
                                else
                                {
                                  co_return co_await m_calcService->AddAsync(input, 10.0);
                                }
                              });
    }

    // ============================================================================
    // Example 7: Batch Operations
    // ============================================================================

    /// @brief Demonstrates launching multiple concurrent operations.
    void DoBatchWork()
    {
      if (!m_calcService)
        return;

      m_operationCount = 0;
      m_expectedCount = 3;

      // Launch multiple operations concurrently
      CallAsync(&ExampleServiceUse::HandleBatchResult, [this]() -> boost::asio::awaitable<double> { return m_calcService->AddAsync(1.0, 2.0); });

      CallAsync(&ExampleServiceUse::HandleBatchResult, [this]() -> boost::asio::awaitable<double> { return m_calcService->AddAsync(5.0, 10.0); });

      CallAsync(&ExampleServiceUse::HandleBatchResult, [this]() -> boost::asio::awaitable<double> { return m_calcService->AddAsync(20.0, 30.0); });
    }

    // ============================================================================
    // Example 8: Capturing State
    // ============================================================================

    /// @brief Demonstrates capturing state in lambda coroutines.
    void DoWorkWithCapture(const std::string& prefix, double value)
    {
      if (!m_calcService)
        return;

      auto future = CallAsync(&ExampleServiceUse::HandleStringResult,
                              [this, prefix, value]() -> boost::asio::awaitable<std::string>
                              {
                                auto result = co_await m_calcService->AddAsync(value, 100.0);
                                co_return prefix + ": " + std::to_string(result);
                              });
    }

  private:
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
        future.get();
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

    std::cout << "=== Example 1: Simple Work ===" << std::endl;
    example->DoSimpleWork();

    std::cout << "\n=== Example 2: Service Call ===" << std::endl;
    example->DoServiceCall();

    std::cout << "\n=== Example 3: Error Handling (Success) ===" << std::endl;
    example->DoWorkThatMightFail(false);

    std::cout << "\n=== Example 4: Error Handling (Failure) ===" << std::endl;
    example->DoWorkThatMightFail(true);

    std::cout << "\n=== Example 5: Void Operation ===" << std::endl;
    example->DoVoidWork("Test message");

    std::cout << "\n=== Example 6: Chained Work ===" << std::endl;
    example->DoChainedWork();

    std::cout << "\n=== Example 7: Complex Work ===" << std::endl;
    example->DoComplexWork(150.0);

    std::cout << "\n=== Example 8: Batch Operations ===" << std::endl;
    example->DoBatchWork();

    std::cout << "\n=== Example 9: Work with Capture ===" << std::endl;
    example->DoWorkWithCapture("Result", 25.0);

    // Run the event loop
    ioc.run();
  }
}    // namespace Test3

#endif
