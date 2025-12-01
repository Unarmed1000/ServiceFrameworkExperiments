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

#include <Test1/AddService.hpp>
#include <Test1/CalculatorService.hpp>
#include <Test1/ComplexService.hpp>
#include <Test1/DivideService.hpp>
#include <Test1/MultiplyService.hpp>
#include <Test1/ServiceBase.hpp>
#include <Test1/SubtractService.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/version.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <gtest/gtest.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

// Using declarations for convenience (in general code, prefer explicit namespaces, this is a small demo so it's fine)
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using namespace boost::asio::experimental::awaitable_operators;

// Test fixture for Service Framework tests
namespace Test1
{
  class ServiceFrameworkTest : public ::testing::Test
  {
  protected:
    std::unique_ptr<boost::asio::io_context> m_io;

    std::unique_ptr<AddService> m_addService;
    std::unique_ptr<MultiplyService> m_multiplyService;
    std::unique_ptr<SubtractService> m_subtractService;
    std::unique_ptr<DivideService> m_divideService;
    std::unique_ptr<ComplexService> m_complexService;
    std::unique_ptr<CalculatorService> m_calculatorService;

    void SetUp() override
    {
      // Initialize spdlog async logger with thread ID in pattern
      static bool spdlog_initialized = false;
      if (!spdlog_initialized)
      {
        spdlog::init_thread_pool(8192, 1);
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto async_logger =
          std::make_shared<spdlog::async_logger>("async_logger", stdout_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        async_logger->set_pattern("[thread %t] %v");
        spdlog::set_default_logger(async_logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        spdlog_initialized = true;
      }

      m_io = std::make_unique<boost::asio::io_context>();

      m_addService = std::make_unique<AddService>();
      m_multiplyService = std::make_unique<MultiplyService>();
      m_subtractService = std::make_unique<SubtractService>();
      m_divideService = std::make_unique<DivideService>();
      m_complexService = std::make_unique<ComplexService>();
      m_calculatorService = std::make_unique<CalculatorService>(*m_addService, *m_multiplyService, *m_subtractService, *m_divideService);

      m_addService->start();
      m_multiplyService->start();
      m_subtractService->start();
      m_divideService->start();
      m_complexService->start();
      m_calculatorService->start();
    }

    void TearDown() override
    {
      m_calculatorService->stop();
      m_complexService->stop();
      m_divideService->stop();
      m_subtractService->stop();
      m_multiplyService->stop();
      m_addService->stop();
    }

    // Helper to run a coroutine and wait for completion
    template <typename T>
    T runCoroutine(awaitable<T> coro)
    {
      auto future = co_spawn(*m_io, std::move(coro), boost::asio::use_future);
      m_io->run();
      m_io->restart();
      return future.get();
    }

    // Overload for void coroutines
    void runCoroutine(awaitable<void> coro)
    {
      auto future = co_spawn(*m_io, std::move(coro), boost::asio::use_future);
      m_io->run();
      m_io->restart();
      future.get();
    }
  };

  // Coroutine test functions
  awaitable<void> SequentialExample(AddService& add, MultiplyService& mul, SubtractService& sub)
  {
    double result1 = co_await add.AddAsync(10, 5);
    EXPECT_DOUBLE_EQ(result1, 15);

    double result2 = co_await mul.MultiplyAsync(7, 3);
    EXPECT_DOUBLE_EQ(result2, 21);

    double result3 = co_await sub.SubtractAsync(20, 8);
    EXPECT_DOUBLE_EQ(result3, 12);
  }

  awaitable<double> ComplexCalculation(AddService& add, MultiplyService& mul, SubtractService& sub)
  {
    double step1 = co_await add.AddAsync(10, 5);    // 15
    EXPECT_DOUBLE_EQ(step1, 15);

    double step2 = co_await mul.MultiplyAsync(step1, 3);    // 45
    EXPECT_DOUBLE_EQ(step2, 45);

    double result = co_await sub.SubtractAsync(step2, 8);    // 37
    EXPECT_DOUBLE_EQ(result, 37);

    co_return result;
  }

  awaitable<std::vector<double>> ConcurrentExample(AddService& add, MultiplyService& mul, SubtractService& sub)
  {
    // Store awaitables in an array, launch them all, then wait
    std::vector<awaitable<double>> operations;
    operations.push_back(add.AddAsync(100, 200));
    operations.push_back(mul.MultiplyAsync(5, 6));
    operations.push_back(sub.SubtractAsync(50, 25));

    // Wait for all to complete
    std::vector<double> results;
    for (auto& op : operations)
    {
      results.push_back(co_await std::move(op));
    }

    co_return results;
  }

  awaitable<double> NestedCalculation(AddService& add, MultiplyService& mul, SubtractService& sub)
  {
    // Calculate sequentially
    double sum = co_await add.AddAsync(10, 5);    // 15
    EXPECT_DOUBLE_EQ(sum, 15);

    double diff = co_await sub.SubtractAsync(7, 3);    // 4
    EXPECT_DOUBLE_EQ(diff, 4);

    double result = co_await mul.MultiplyAsync(sum, diff);    // 60
    EXPECT_DOUBLE_EQ(result, 60);

    co_return result;
  }

  awaitable<std::string> StuffAsync(ComplexService& complex, std::unique_ptr<Common::ComplexData> data)
  {
    std::string res = co_await complex.StuffAsync(std::move(data));
    co_return res;
  }

  awaitable<std::string> StuffWithExceptionAsync(ComplexService& complex, std::unique_ptr<Common::ComplexData> data)
  {
    std::string res = co_await complex.StuffWithExceptionAsync(std::move(data));
    co_return res;
  }
}


using namespace Test1;

// Main test that runs all examples together
TEST_F(ServiceFrameworkTest, MainWorkflow)
{
  spdlog::info("=== Service Framework Test 1 ===");
  spdlog::info("Boost version: {}.{}.{}", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
  spdlog::info("Main thread: {}", fmt::streamed(std::this_thread::get_id()));

  runCoroutine(
    [&]() -> awaitable<void>
    {
      co_await SequentialExample(*m_addService, *m_multiplyService, *m_subtractService);
      double complexResult = co_await ComplexCalculation(*m_addService, *m_multiplyService, *m_subtractService);
      EXPECT_DOUBLE_EQ(complexResult, 37);

      auto concurrentResults = co_await ConcurrentExample(*m_addService, *m_multiplyService, *m_subtractService);
      EXPECT_EQ(concurrentResults.size(), 3);

      double nestedResult = co_await NestedCalculation(*m_addService, *m_multiplyService, *m_subtractService);
      EXPECT_DOUBLE_EQ(nestedResult, 60);
    }());
}

// Individual tests for each coroutine
TEST_F(ServiceFrameworkTest, SequentialOperations)
{
  using namespace Test1;

  runCoroutine(SequentialExample(*m_addService, *m_multiplyService, *m_subtractService));
}

TEST_F(ServiceFrameworkTest, ComplexCalculation2)
{
  double result = runCoroutine(ComplexCalculation(*m_addService, *m_multiplyService, *m_subtractService));
  EXPECT_DOUBLE_EQ(result, 37);
  spdlog::info("Complex calculation result: {}", result);
}

TEST_F(ServiceFrameworkTest, ConcurrentOperations)
{
  auto results = runCoroutine(ConcurrentExample(*m_addService, *m_multiplyService, *m_subtractService));
  ASSERT_EQ(results.size(), 3);
  EXPECT_DOUBLE_EQ(results[0], 300);    // 100 + 200
  EXPECT_DOUBLE_EQ(results[1], 30);     // 5 * 6
  EXPECT_DOUBLE_EQ(results[2], 25);     // 50 - 25
  spdlog::info("Concurrent results: {}, {}, {}", results[0], results[1], results[2]);
}


TEST_F(ServiceFrameworkTest, NestedCalculation)
{
  double result = runCoroutine(NestedCalculation(*m_addService, *m_multiplyService, *m_subtractService));
  EXPECT_DOUBLE_EQ(result, 60);
  spdlog::info("Nested calculation result: {}", result);
}


TEST_F(ServiceFrameworkTest, StuffAsync)
{
  const std::string name = "TestName";
  const std::string result = runCoroutine(StuffAsync(*m_complexService, std::make_unique<Common::ComplexData>(name, "SomeValue")));

  EXPECT_EQ(result, fmt::format("{}_processed", name));
  spdlog::info("Nested calculation result: {}", result);
}


TEST_F(ServiceFrameworkTest, StuffWithException)
{
  EXPECT_THROW(runCoroutine(StuffWithExceptionAsync(*m_complexService, std::make_unique<Common::ComplexData>("TestName", "SomeValue"))),
               ComplexService::TestException);
}


TEST_F(ServiceFrameworkTest, DivideService)
{
  double result = runCoroutine(m_divideService->DivideAsync(10, 2));
  EXPECT_DOUBLE_EQ(result, 5.0);
  spdlog::info("Division result: {}", result);

  result = runCoroutine(m_divideService->DivideAsync(7, 2));
  EXPECT_NEAR(result, 3.5, 0.0001);
  spdlog::info("Division result: {}", result);

  // Test division by zero
  EXPECT_THROW(runCoroutine(m_divideService->DivideAsync(10, 0)), std::runtime_error);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceBasic)
{
  // Basic operations
  double result = runCoroutine(m_calculatorService->EvaluateAsync("1+1"));
  EXPECT_DOUBLE_EQ(result, 2.0);
  spdlog::info("1+1 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("10-5"));
  EXPECT_DOUBLE_EQ(result, 5.0);
  spdlog::info("10-5 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("3*4"));
  EXPECT_DOUBLE_EQ(result, 12.0);
  spdlog::info("3*4 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("20/4"));
  EXPECT_DOUBLE_EQ(result, 5.0);
  spdlog::info("20/4 = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceOperatorPrecedence)
{
  // Test order of operations
  double result = runCoroutine(m_calculatorService->EvaluateAsync("1+2*3"));
  EXPECT_DOUBLE_EQ(result, 7.0);
  spdlog::info("1+2*3 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("10-2*3"));
  EXPECT_DOUBLE_EQ(result, 4.0);
  spdlog::info("10-2*3 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("20/4+3"));
  EXPECT_DOUBLE_EQ(result, 8.0);
  spdlog::info("20/4+3 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("2+3*4-5"));
  EXPECT_DOUBLE_EQ(result, 9.0);
  spdlog::info("2+3*4-5 = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceParentheses)
{
  // Test parentheses
  double result = runCoroutine(m_calculatorService->EvaluateAsync("(1+2)*3"));
  EXPECT_DOUBLE_EQ(result, 9.0);
  spdlog::info("(1+2)*3 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("(5+3)*2"));
  EXPECT_DOUBLE_EQ(result, 16.0);
  spdlog::info("(5+3)*2 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("10*(2+3)"));
  EXPECT_DOUBLE_EQ(result, 50.0);
  spdlog::info("10*(2+3) = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("(10-2)*(3+4)"));
  EXPECT_DOUBLE_EQ(result, 56.0);
  spdlog::info("(10-2)*(3+4) = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceNestedParentheses)
{
  // Test nested parentheses
  double result = runCoroutine(m_calculatorService->EvaluateAsync("((1+2)*3)"));
  EXPECT_DOUBLE_EQ(result, 9.0);
  spdlog::info("((1+2)*3) = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("(2*(3+(4*5)))"));
  EXPECT_DOUBLE_EQ(result, 46.0);
  spdlog::info("(2*(3+(4*5))) = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("((10+5)*2)/(3+2)"));
  EXPECT_DOUBLE_EQ(result, 6.0);
  spdlog::info("((10+5)*2)/(3+2) = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceWhitespace)
{
  // Test with whitespace
  double result = runCoroutine(m_calculatorService->EvaluateAsync("1 + 1"));
  EXPECT_DOUBLE_EQ(result, 2.0);
  spdlog::info("1 + 1 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("  ( 5  +  3 ) * 2  "));
  EXPECT_DOUBLE_EQ(result, 16.0);
  spdlog::info("  ( 5  +  3 ) * 2   = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceNegativeNumbers)
{
  // Test negative numbers
  double result = runCoroutine(m_calculatorService->EvaluateAsync("(-5)+3"));
  EXPECT_DOUBLE_EQ(result, -2.0);
  spdlog::info("(-5)+3 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("10+(-5)"));
  EXPECT_DOUBLE_EQ(result, 5.0);
  spdlog::info("10+(-5) = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("(-2)*(-3)"));
  EXPECT_DOUBLE_EQ(result, 6.0);
  spdlog::info("(-2)*(-3) = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceConcurrent)
{
  // Test concurrent calculator evaluations
  auto results = runCoroutine(
    [&]() -> awaitable<std::vector<double>>
    {
      // Launch four calculator evaluations concurrently
      std::vector<awaitable<double>> operations;
      operations.push_back(m_calculatorService->EvaluateAsync("10+20"));          // 30
      operations.push_back(m_calculatorService->EvaluateAsync("(5+3)*2"));        // 16
      operations.push_back(m_calculatorService->EvaluateAsync("100/4-5"));        // 20
      operations.push_back(m_calculatorService->EvaluateAsync("2*(3+(4*5))"));    // 46

      // Wait for all to complete
      std::vector<double> results;
      for (auto& op : operations)
      {
        results.push_back(co_await std::move(op));
      }

      co_return results;
    }());

  ASSERT_EQ(results.size(), 4);
  EXPECT_DOUBLE_EQ(results[0], 30.0);
  EXPECT_DOUBLE_EQ(results[1], 16.0);
  EXPECT_DOUBLE_EQ(results[2], 20.0);
  EXPECT_DOUBLE_EQ(results[3], 46.0);
  spdlog::info("Concurrent calculator results: {}, {}, {}, {}", results[0], results[1], results[2], results[3]);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceDecimals)
{
  // Test decimal numbers
  double result = runCoroutine(m_calculatorService->EvaluateAsync("1.5+2.5"));
  EXPECT_DOUBLE_EQ(result, 4.0);
  spdlog::info("1.5+2.5 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("3.14*2"));
  EXPECT_NEAR(result, 6.28, 0.0001);
  spdlog::info("3.14*2 = {}", result);

  result = runCoroutine(m_calculatorService->EvaluateAsync("10.5/2.5"));
  EXPECT_NEAR(result, 4.2, 0.0001);
  spdlog::info("10.5/2.5 = {}", result);
}


TEST_F(ServiceFrameworkTest, CalculatorServiceErrors)
{
  // Test parse errors
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("1++2")), std::invalid_argument);
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("(1+2")), std::invalid_argument);
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("1+2)")), std::invalid_argument);
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("")), std::invalid_argument);
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("abc")), std::invalid_argument);

  // Test division by zero
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("10/0")), std::runtime_error);
  EXPECT_THROW(runCoroutine(m_calculatorService->EvaluateAsync("5/(3-3)")), std::runtime_error);
}
