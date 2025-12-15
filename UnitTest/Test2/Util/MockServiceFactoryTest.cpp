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

#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Util/MockServiceFactory.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

using namespace Test2;
using namespace Test2::UnitTest;

// ==============================================
// MockService Tests (Services have InitAsync/ShutdownAsync)
// ==============================================

TEST(MockServiceTest, InitAsync_Success_SetsInitializedFlag)
{
  MockServiceConfig config("TestService");

  boost::asio::io_context ioContext;
  bool initCompleted = false;

  boost::asio::co_spawn(
    ioContext.get_executor(),
    [&config, &initCompleted]() -> boost::asio::awaitable<void>
    {
      std::weak_ptr<IServiceProvider> emptyProvider;
      ServiceProvider provider(emptyProvider);
      ServiceCreateInfo createInfo(provider);

      MockService service(createInfo, config);
      auto result = co_await service.InitAsync(createInfo);
      EXPECT_EQ(result, ServiceInitResult::Success);
      EXPECT_TRUE(service.IsInitialized());
      initCompleted = true;
    },
    boost::asio::detached);

  ioContext.run();
  EXPECT_TRUE(initCompleted);
}

TEST(MockServiceTest, InitAsync_WithFailure_ThrowsException)
{
  MockServiceConfig config("FailService");
  config.InitShouldFail = true;
  config.InitErrorMessage = "Test init failure";

  boost::asio::io_context ioContext;
  bool exceptionCaught = false;

  boost::asio::co_spawn(
    ioContext.get_executor(),
    [&config, &exceptionCaught]() -> boost::asio::awaitable<void>
    {
      try
      {
        std::weak_ptr<IServiceProvider> emptyProvider;
        ServiceProvider provider(emptyProvider);
        ServiceCreateInfo createInfo(provider);

        MockService service(createInfo, config);
        co_await service.InitAsync(createInfo);
      }
      catch (const std::runtime_error& e)
      {
        EXPECT_STREQ(e.what(), "Test init failure");
        exceptionCaught = true;
      }
    },
    boost::asio::detached);

  ioContext.run();
  EXPECT_TRUE(exceptionCaught);
}

TEST(MockServiceTest, InitAsync_WithTracker_RecordsInitialization)
{
  InitializationOrderTracker tracker;
  MockServiceConfig config("TrackedService");
  config.InitTracker = &tracker;

  boost::asio::io_context ioContext;

  boost::asio::co_spawn(
    ioContext.get_executor(),
    [&config]() -> boost::asio::awaitable<void>
    {
      std::weak_ptr<IServiceProvider> emptyProvider;
      ServiceProvider provider(emptyProvider);
      ServiceCreateInfo createInfo(provider);

      MockService service(createInfo, config);
      co_await service.InitAsync(createInfo);
    },
    boost::asio::detached);

  ioContext.run();

  ASSERT_EQ(tracker.Order.size(), 1);
  EXPECT_EQ(tracker.Order[0], "TrackedService");
}

TEST(MockServiceTest, ShutdownAsync_Success_SetsShutdownFlag)
{
  MockServiceConfig config("ShutdownService");

  boost::asio::io_context ioContext;
  bool shutdownCompleted = false;

  boost::asio::co_spawn(
    ioContext.get_executor(),
    [&config, &shutdownCompleted]() -> boost::asio::awaitable<void>
    {
      std::weak_ptr<IServiceProvider> emptyProvider;
      ServiceProvider provider(emptyProvider);
      ServiceCreateInfo createInfo(provider);

      MockService service(createInfo, config);
      co_await service.InitAsync(createInfo);

      auto result = co_await service.ShutdownAsync();
      EXPECT_EQ(result, ServiceShutdownResult::Success);
      EXPECT_TRUE(service.IsShutdown());
      shutdownCompleted = true;
    },
    boost::asio::detached);

  ioContext.run();
  EXPECT_TRUE(shutdownCompleted);
}

// ==============================================
// MockServiceProxy Tests (Proxies do NOT have InitAsync/ShutdownAsync)
// ==============================================

TEST(MockServiceProxyTest, Constructor_WithTracker_RecordsWithProxySuffix)
{
  InitializationOrderTracker tracker;
  MockServiceConfig config("TrackedProxy");
  config.InitTracker = &tracker;

  std::weak_ptr<IServiceProvider> emptyProvider;
  ServiceProvider provider(emptyProvider);
  boost::asio::io_context ioContext;
  ExecutorContext<ILifeTracker> sourceContext(std::shared_ptr<ILifeTracker>(), ioContext.get_executor());
  ExecutorContext<IService> targetContext(std::shared_ptr<IService>(), ioContext.get_executor());
  DispatchContext dispatchContext(sourceContext, targetContext);
  ServiceProxyCreateInfo createInfo(dispatchContext, provider);    // NOTE: DispatchContext first, then provider

  MockServiceProxy proxy(createInfo, config);

  // Proxy construction should record with "_Proxy" suffix
  ASSERT_EQ(tracker.Order.size(), 1);
  EXPECT_EQ(tracker.Order[0], "TrackedProxy_Proxy");
}

TEST(MockServiceProxyTest, Constructor_RecordsThreadId)
{
  MockServiceConfig config("ProxyThreadTest");

  std::weak_ptr<IServiceProvider> emptyProvider;
  ServiceProvider provider(emptyProvider);
  boost::asio::io_context ioContext;
  ExecutorContext<ILifeTracker> sourceContext(std::shared_ptr<ILifeTracker>(), ioContext.get_executor());
  ExecutorContext<IService> targetContext(std::shared_ptr<IService>(), ioContext.get_executor());
  DispatchContext dispatchContext(sourceContext, targetContext);
  ServiceProxyCreateInfo createInfo(dispatchContext, provider);    // NOTE: DispatchContext first, then provider

  auto constructThreadId = std::this_thread::get_id();
  MockServiceProxy proxy(createInfo, config);

  EXPECT_EQ(proxy.GetConstructThreadId(), constructThreadId);
}

// ==============================================
// InitializationOrderTracker Tests
// ==============================================

TEST(InitializationOrderTrackerTest, RecordInit_AddsToOrder)
{
  InitializationOrderTracker tracker;
  tracker.RecordInit("Service1");
  tracker.RecordInit("Service2");
  tracker.RecordInit("Service3");

  ASSERT_EQ(tracker.Order.size(), 3);
  EXPECT_EQ(tracker.Order[0], "Service1");
  EXPECT_EQ(tracker.Order[1], "Service2");
  EXPECT_EQ(tracker.Order[2], "Service3");
}

TEST(InitializationOrderTrackerTest, Clear_RemovesAllEntries)
{
  InitializationOrderTracker tracker;
  tracker.RecordInit("Service1");
  tracker.RecordInit("Service2");

  tracker.Clear();

  EXPECT_TRUE(tracker.Order.empty());
}

// ==============================================
// Factory Tests
// ==============================================

TEST(MockServiceImplFactoryTest, Create_ReturnsValidService)
{
  MockServiceConfig config("FactoryService");
  MockServiceImplFactory factory(config);

  std::weak_ptr<IServiceProvider> emptyProvider;
  ServiceProvider provider(emptyProvider);
  ServiceCreateInfo createInfo(provider);

  auto service = factory.Create(createInfo);
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(std::dynamic_pointer_cast<MockService>(service)->IsInitialized());
}

TEST(MockServiceProxyFactoryTest, CreateProxy_ReturnsValidProxy)
{
  MockServiceConfig config("FactoryProxy");
  MockServiceProxyFactory factory(config);

  std::weak_ptr<IServiceProvider> emptyProvider;
  ServiceProvider provider(emptyProvider);
  boost::asio::io_context ioContext;
  ExecutorContext<ILifeTracker> sourceContext(std::shared_ptr<ILifeTracker>(), ioContext.get_executor());
  ExecutorContext<IService> targetContext(std::shared_ptr<IService>(), ioContext.get_executor());
  DispatchContext dispatchContext(sourceContext, targetContext);
  ServiceProxyCreateInfo createInfo(dispatchContext, provider);    // NOTE: DispatchContext first, then provider

  auto proxy = factory.CreateProxy(typeid(ITestInterface), createInfo);
  ASSERT_NE(proxy, nullptr);
}
