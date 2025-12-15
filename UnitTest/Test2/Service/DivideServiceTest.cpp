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

#include <Test2/Framework/Config/ThreadGroupConfig.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManager.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManagerConfig.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Registry/ServiceRegistry.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceFactoryUtil.hpp>
#include <Test2/Services/Divide/DivideServiceImplFactory.hpp>
#include <Test2/Services/Divide/DivideServiceProxyFactory.hpp>
#include <Test2/Services/Divide/IDivideService.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <vector>

namespace Test2
{
  // ============================================================================
  // Helper Functions
  // ============================================================================

  // Helper to run async operations synchronously using polling
  void RunAsyncWithPolling(LifecycleManager& manager, std::function<boost::asio::awaitable<void>()> asyncOp)
  {
    bool done = false;
    std::exception_ptr exceptionPtr;

    // Spawn the coroutine
    auto spawnOp = [&]() -> boost::asio::awaitable<void>
    {
      try
      {
        co_await asyncOp();
      }
      catch (...)
      {
        exceptionPtr = std::current_exception();
      }
      done = true;
    };

    // Use a local io_context for the test
    boost::asio::io_context testContext;
    boost::asio::co_spawn(testContext, spawnOp(), boost::asio::detached);

    // Keep polling both contexts until the operation is complete
    while (!done)
    {
      testContext.poll();
      manager.Poll();
    }

    if (exceptionPtr)
    {
      std::rethrow_exception(exceptionPtr);
    }
  }

  // ============================================================================
  // DivideService Test Fixture
  // ============================================================================

  class DivideServiceTest : public ::testing::Test
  {
  protected:
    std::unique_ptr<LifecycleManager> manager;

    void SetUp() override
    {
      // Create service registry
      ServiceRegistry registry;

      // Register Divide service on main thread group
      registry.RegisterService(AsyncServiceFactoryUtil::CreateAsyncServiceFactory<DivideServiceProxyFactory, DivideServiceImplFactory>(),
                               ServiceLaunchPriority(100), ThreadGroupConfig::MainThreadGroupId);

      // Extract registrations and create lifecycle manager
      auto registrations = registry.ExtractRegistrations();
      LifecycleManagerConfig config;
      manager = std::make_unique<LifecycleManager>(config, std::move(registrations));

      // Start services
      RunAsyncWithPolling(*manager, [this]() -> boost::asio::awaitable<void> { co_await manager->StartServicesAsync(); });
    }

    void TearDown() override
    {
      if (manager)
      {
        // Shutdown services
        RunAsyncWithPolling(*manager, [this]() -> boost::asio::awaitable<void> { co_await manager->ShutdownServicesAsync(); });
        manager.reset();
      }
    }
  };

  // ============================================================================
  // DivideService Lifecycle Tests
  // ============================================================================

  TEST_F(DivideServiceTest, UpdateProcessesWithoutBlocking)
  {
    // Process services - this calls Process() on all main thread services
    auto result = manager->Update();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  TEST_F(DivideServiceTest, MultipleUpdatesSucceed)
  {
    // Call Update multiple times
    for (int i = 0; i < 5; ++i)
    {
      auto result = manager->Update();
      EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
    }
  }

  TEST_F(DivideServiceTest, DivideAsyncReturnsCorrectResult)
  {
    // Get the Divide service proxy
    auto serviceProvider = manager->GetServiceProvider();
    auto divideService = serviceProvider.TryGetService<IDivideService>();
    ASSERT_NE(divideService, nullptr);

    // Call the async method
    auto future = boost::asio::co_spawn(manager->GetExecutor(), divideService->DivideAsync(20.0, 4.0), boost::asio::use_future);

    // Poll until the operation completes
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
      manager->Update();
    }

    // Get the result
    double result = future.get();
    EXPECT_DOUBLE_EQ(result, 5.0);
  }

  TEST_F(DivideServiceTest, DivideAsyncHandlesMultipleOperations)
  {
    // Get the Divide service proxy
    auto serviceProvider = manager->GetServiceProvider();
    auto divideService = serviceProvider.TryGetService<IDivideService>();
    ASSERT_NE(divideService, nullptr);

    // Call multiple operations
    auto future = boost::asio::co_spawn(
      manager->GetExecutor(),
      [&]() -> boost::asio::awaitable<std::vector<double>>
      {
        std::vector<double> results;
        results.push_back(co_await divideService->DivideAsync(100.0, 5.0));
        results.push_back(co_await divideService->DivideAsync(200.0, 25.0));
        results.push_back(co_await divideService->DivideAsync(15.0, 3.0));
        results.push_back(co_await divideService->DivideAsync(10.0, 2.5));
        co_return results;
      }(),
      boost::asio::use_future);

    // Poll until complete
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
      manager->Update();
    }

    // Get the results
    auto results = future.get();
    ASSERT_EQ(results.size(), 4u);
    EXPECT_DOUBLE_EQ(results[0], 20.0);
    EXPECT_DOUBLE_EQ(results[1], 8.0);
    EXPECT_DOUBLE_EQ(results[2], 5.0);
    EXPECT_DOUBLE_EQ(results[3], 4.0);
  }
}
