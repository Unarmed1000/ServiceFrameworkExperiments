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
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Registry/ServiceRegistry.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceFactoryUtil.hpp>
#include <Test2/Services/Add/AddServiceImplFactory.hpp>
#include <Test2/Services/Add/AddServiceProxyFactory.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <functional>
#include <memory>

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
  // AddService Test Fixture
  // ============================================================================

  class AddServiceTest : public ::testing::Test
  {
  protected:
    std::unique_ptr<LifecycleManager> manager;

    void SetUp() override
    {
      // Create service registry
      ServiceRegistry registry;

      // Register Add service on main thread group
      registry.RegisterService(AsyncServiceFactoryUtil::CreateAsyncServiceFactory<AddServiceProxyFactory, AddServiceImplFactory>(),
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
  // AddService Lifecycle Tests
  // ============================================================================

  TEST_F(AddServiceTest, MainHostIsValid)
  {
    // Verify main host is valid
    auto& mainHost = manager->GetMainHost();
    EXPECT_NE(&mainHost, nullptr);
  }

  TEST_F(AddServiceTest, UpdateProcessesWithoutBlocking)
  {
    // Process services - this calls Process() on all main thread services
    auto result = manager->Update();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  TEST_F(AddServiceTest, MultipleUpdatesSucceed)
  {
    // Call Update multiple times
    for (int i = 0; i < 5; ++i)
    {
      auto result = manager->Update();
      EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
    }
  }
}
