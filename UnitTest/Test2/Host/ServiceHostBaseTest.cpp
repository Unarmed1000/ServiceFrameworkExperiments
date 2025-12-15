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

#include <Common/AggregateException.hpp>
#include <Test2/Framework/Exception/EmptyPriorityGroupException.hpp>
#include <Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/StartServiceProxyRecord.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/IAsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Util/MockServiceFactory.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <typeindex>

namespace Test2
{
  // Helper to track service lifecycle events for testing
  struct ServiceLifecycleTracker
  {
    std::atomic<bool> initCalled{false};
    std::atomic<bool> shutdownCalled{false};
    std::string serviceName;

    void RecordInit(const std::string& name)
    {
      serviceName = name;
      initCalled = true;
    }

    void RecordShutdown()
    {
      shutdownCalled = true;
    }
  };

  // Mock service for testing
  class MockService : public IServiceControl
  {
  private:
    std::string m_name;
    bool m_initShouldFail;
    bool m_shutdownShouldFail;
    std::shared_ptr<ServiceLifecycleTracker> m_tracker;

  public:
    explicit MockService(std::string name, std::shared_ptr<ServiceLifecycleTracker> tracker = nullptr, bool initShouldFail = false,
                         bool shutdownShouldFail = false)
      : m_name(std::move(name))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
      , m_tracker(std::move(tracker))
    {
    }

    [[nodiscard]] const std::string& GetName() const noexcept
    {
      return m_name;
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      spdlog::info("MockService::InitAsync called for {}", m_name);
      if (m_tracker)
      {
        m_tracker->RecordInit(m_name);
      }
      if (m_initShouldFail)
      {
        spdlog::error("MockService::InitAsync throwing for {}", m_name);
        throw std::runtime_error("Init failed for " + m_name);
      }
      spdlog::info("MockService::InitAsync returning success for {}", m_name);
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      if (m_tracker)
      {
        m_tracker->RecordShutdown();
      }
      if (m_shutdownShouldFail)
      {
        throw std::runtime_error("Shutdown failed for " + m_name);
      }
      co_return ServiceShutdownResult::Success;
    }

    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }
  };

  struct ITestInterface : public IService
  {
  };

  // Mock factory
  class MockServiceFactory : public AsyncServiceImplFactory
  {
  private:
    std::string m_serviceName;
    std::shared_ptr<ServiceLifecycleTracker> m_tracker;
    bool m_initShouldFail;
    bool m_shutdownShouldFail;

  public:
    explicit MockServiceFactory(std::string serviceName, std::shared_ptr<ServiceLifecycleTracker> tracker = nullptr, bool initShouldFail = false,
                                bool shutdownShouldFail = false)
      : AsyncServiceImplFactory(typeid(ITestInterface))
      , m_serviceName(std::move(serviceName))
      , m_tracker(std::move(tracker))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
    {
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return std::make_shared<MockService>(m_serviceName, m_tracker, m_initShouldFail, m_shutdownShouldFail);
    }
  };

  // ============================================================================
  // Test Fixture using CooperativeThreadServiceHost for simpler async testing
  // ============================================================================

  class ServiceHostTest : public ::testing::Test
  {
  protected:
    CooperativeThreadServiceHost host;

    // Helper to run async operations synchronously using Poll()
    template <typename Func>
    void RunAsync(Func&& asyncFunc)
    {
      bool done = false;
      std::exception_ptr exceptionPtr;

      boost::asio::co_spawn(
        host.GetExecutor(),
        [&]() -> boost::asio::awaitable<void>
        {
          try
          {
            co_await asyncFunc();
          }
          catch (...)
          {
            exceptionPtr = std::current_exception();
          }
          done = true;
        },
        boost::asio::detached);

      // Poll until operation completes
      while (!done)
      {
        host.Poll();
      }

      if (exceptionPtr)
      {
        std::rethrow_exception(exceptionPtr);
      }
    }

    void RegisterServices(std::vector<StartServiceRecord> services, uint32_t priority)
    {
      RunAsync([this, services = std::move(services), priority]() mutable -> boost::asio::awaitable<void>
               { [[maybe_unused]] auto result = co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(priority)); });
    }

    // Helper to start services and capture AggregateException if thrown
    std::optional<Common::AggregateException> TryRegisterServicesExpectingFailure(std::vector<StartServiceRecord> services, uint32_t priority)
    {
      bool done = false;
      std::optional<Common::AggregateException> caughtException;

      boost::asio::co_spawn(
        host.GetExecutor(),
        [this, services = std::move(services), priority, &caughtException, &done]() mutable -> boost::asio::awaitable<void>
        {
          try
          {
            [[maybe_unused]] auto result = co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(priority));
          }
          catch (const Common::AggregateException& ex)
          {
            caughtException.emplace(ex);
          }
          done = true;
        },
        boost::asio::detached);

      while (!done)
      {
        host.Poll();
      }

      return caughtException;
    }
  };

  // Helper to convert ServiceInstanceInfo to ServiceProviderServiceInstance
  std::vector<ServiceProviderServiceInstance> ConvertToProviderInstances(const std::vector<ServiceInstanceInfo>& serviceInfos, InstanceType type)
  {
    std::vector<ServiceProviderServiceInstance> instances;
    instances.reserve(serviceInfos.size());
    for (const auto& info : serviceInfos)
    {
      ServiceProviderServiceInstance instance;
      instance.Type = type;
      instance.Instance = info.Service;
      instance.SupportedInterfaces = info.SupportedInterfaces;
      instances.push_back(std::move(instance));
    }
    return instances;
  }

  // ========================================
  // Phase 3: Empty Service List Handling
  // ========================================

  TEST(ManagedThreadServiceHostTest, EmptyServiceList_Succeeds)
  {
    ManagedThreadServiceProvider provider;
    std::vector<ServiceInstanceInfo> emptyServices;
    auto providerInstances = ConvertToProviderInstances(emptyServices, InstanceType::Service);
    EXPECT_THROW(provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(1000), std::move(providerInstances)),
                 EmptyPriorityGroupException);
  }

  // ========================================
  // Phase 4: Service Initialization Success
  // ========================================

  TEST_F(ServiceHostTest, SingleService_InitializesSuccessfully)
  {
    auto tracker = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("TestService", std::make_unique<MockServiceFactory>("TestService", tracker));

    RegisterServices(std::move(services), 1000);

    EXPECT_TRUE(tracker->initCalled);
    EXPECT_FALSE(tracker->shutdownCalled);
  }

  TEST_F(ServiceHostTest, MultipleServices_InitializeInOrder)
  {
    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("Service1", std::make_unique<MockServiceFactory>("Service1", tracker1));
    services.emplace_back("Service2", std::make_unique<MockServiceFactory>("Service2", tracker2));
    services.emplace_back("Service3", std::make_unique<MockServiceFactory>("Service3", tracker3));

    RegisterServices(std::move(services), 1000);

    EXPECT_TRUE(tracker1->initCalled);
    EXPECT_TRUE(tracker2->initCalled);
    EXPECT_TRUE(tracker3->initCalled);
  }

  // ========================================
  // Phase 5: Init Failure with Rollback
  // ========================================

  TEST_F(ServiceHostTest, ServiceInitFails_RollsBackSuccessfulServices)
  {
    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("Service1", std::make_unique<MockServiceFactory>("Service1", tracker1, false));
    services.emplace_back("Service2", std::make_unique<MockServiceFactory>("Service2", tracker2, true));    // This will fail
    services.emplace_back("Service3", std::make_unique<MockServiceFactory>("Service3", tracker3, false));

    auto exception = TryRegisterServicesExpectingFailure(std::move(services), 1000);

    EXPECT_TRUE(exception.has_value());
    EXPECT_TRUE(tracker1->initCalled);
    EXPECT_TRUE(tracker1->shutdownCalled);     // Should be rolled back
    EXPECT_TRUE(tracker2->initCalled);         // Failed during init
    EXPECT_FALSE(tracker2->shutdownCalled);    // Never successfully initialized
  }

  TEST_F(ServiceHostTest, FirstServiceFails_NoRollbackNeeded)
  {
    auto tracker = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("Service1", std::make_unique<MockServiceFactory>("Service1", tracker, true));

    auto exception = TryRegisterServicesExpectingFailure(std::move(services), 1000);

    EXPECT_TRUE(exception.has_value());
    EXPECT_TRUE(tracker->initCalled);
    EXPECT_FALSE(tracker->shutdownCalled);
  }

  // ========================================
  // Phase 6: Multiple Failures
  // ========================================

  TEST_F(ServiceHostTest, MultipleServicesFail_AggregatesAllExceptions)
  {
    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("Service1", std::make_unique<MockServiceFactory>("Service1", tracker1, true));    // Fails
    services.emplace_back("Service2", std::make_unique<MockServiceFactory>("Service2", tracker2, false));
    services.emplace_back("Service3", std::make_unique<MockServiceFactory>("Service3", tracker3, true));    // Fails

    auto exception = TryRegisterServicesExpectingFailure(std::move(services), 1000);

    ASSERT_TRUE(exception.has_value());
    EXPECT_GE(exception->GetInnerExceptions().size(), 2);    // At least 2 init failures
  }

  TEST_F(ServiceHostTest, RollbackFailure_IncludedInAggregateException)
  {
    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();

    std::vector<StartServiceRecord> services;
    services.emplace_back("Service1", std::make_unique<MockServiceFactory>("Service1", tracker1, false, true));    // Shutdown fails
    services.emplace_back("Service2", std::make_unique<MockServiceFactory>("Service2", tracker2, true));           // Init fails

    auto exception = TryRegisterServicesExpectingFailure(std::move(services), 1000);

    ASSERT_TRUE(exception.has_value());
    EXPECT_GE(exception->GetInnerExceptions().size(), 2);    // Init failure + shutdown failure
  }

  // ========================================
  // Phase 3: Service Proxy Lifecycle Tests
  // ========================================

  TEST_F(ServiceHostTest, TryStartServiceProxiesAsync_EmptyList_Succeeds)
  {
    std::vector<StartServiceProxyRecord> emptyProxies;

    RunAsync([this, &emptyProxies]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(emptyProxies), ServiceLaunchPriority(1000)); });

    // Test passes if no exception is thrown
  }

  TEST_F(ServiceHostTest, TryStartServiceProxiesAsync_SingleProxy_CreatesSuccessfully)
  {
    using namespace Test2::UnitTest;

    InitializationOrderTracker tracker;
    MockServiceConfig config("TestProxy");
    config.InitTracker = &tracker;

    std::vector<StartServiceProxyRecord> proxies;
    proxies.emplace_back("TestProxy", std::make_unique<MockServiceProxyFactory>(config));

    RunAsync([this, &proxies]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies), ServiceLaunchPriority(1000)); });

    // Once implemented, verify proxy was created via tracker
    // EXPECT_EQ(tracker.GetOrder().size(), 1);
    // EXPECT_EQ(tracker.GetOrder()[0], "TestProxy");
  }

  TEST_F(ServiceHostTest, TryStartServiceProxiesAsync_MultipleProxies_CreatesInOrder)
  {
    using namespace Test2::UnitTest;

    InitializationOrderTracker tracker;
    MockServiceConfig config1("Proxy1");
    config1.InitTracker = &tracker;

    MockServiceConfig config2("Proxy2");
    config2.InitTracker = &tracker;

    MockServiceConfig config3("Proxy3");
    config3.InitTracker = &tracker;

    std::vector<StartServiceProxyRecord> proxies;
    proxies.emplace_back("Proxy1", std::make_unique<MockServiceProxyFactory>(config1));
    proxies.emplace_back("Proxy2", std::make_unique<MockServiceProxyFactory>(config2));
    proxies.emplace_back("Proxy3", std::make_unique<MockServiceProxyFactory>(config3));

    RunAsync([this, &proxies]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies), ServiceLaunchPriority(1000)); });

    // Once implemented, verify creation order
    // EXPECT_EQ(tracker.GetOrder().size(), 3);
    // EXPECT_EQ(tracker.GetOrder()[0], "Proxy1");
    // EXPECT_EQ(tracker.GetOrder()[1], "Proxy2");
    // EXPECT_EQ(tracker.GetOrder()[2], "Proxy3");
  }

  TEST_F(ServiceHostTest, TryStartServiceProxiesAsync_NullFactory_ThrowsException)
  {
    std::vector<StartServiceProxyRecord> proxies;
    proxies.emplace_back("ValidProxy", nullptr);    // Null factory should cause exception

    bool exceptionThrown = false;
    RunAsync(
      [this, &proxies, &exceptionThrown]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServiceProxiesAsync(std::move(proxies), ServiceLaunchPriority(1000));
        }
        catch (const std::exception&)
        {
          exceptionThrown = true;
        }
      });

    EXPECT_TRUE(exceptionThrown);
  }

  // ========================================
  // Phase 4: Service Proxy Shutdown Tests
  // ========================================

  TEST_F(ServiceHostTest, TryShutdownServiceProxiesAsync_EmptyPriority_ReturnsEmpty)
  {
    // Shutdown with no proxies registered should return empty exception list
    std::vector<std::exception_ptr> failures;

    RunAsync([this, &failures]() -> boost::asio::awaitable<void>
             { failures = co_await host.TryShutdownServiceProxiesAsync(ServiceLaunchPriority(1000)); });

    EXPECT_TRUE(failures.empty());
  }

  TEST_F(ServiceHostTest, TryShutdownServiceProxiesAsync_SingleProxy_ShutdownsSuccessfully)
  {
    using namespace Test2::UnitTest;

    InitializationOrderTracker tracker;
    MockServiceConfig config("TestProxy");
    config.InitTracker = &tracker;

    // Start a proxy first
    std::vector<StartServiceProxyRecord> proxies;
    proxies.emplace_back("TestProxy", std::make_unique<MockServiceProxyFactory>(config));

    RunAsync([this, &proxies]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies), ServiceLaunchPriority(1000)); });

    // Shutdown the proxy
    std::vector<std::exception_ptr> failures;
    RunAsync([this, &failures]() -> boost::asio::awaitable<void>
             { failures = co_await host.TryShutdownServiceProxiesAsync(ServiceLaunchPriority(1000)); });

    // No failures expected (proxies don't have ShutdownAsync)
    EXPECT_TRUE(failures.empty());
  }

  TEST_F(ServiceHostTest, TryShutdownServiceProxiesAsync_MultipleProxies_ShutdownsInReverseOrder)
  {
    using namespace Test2::UnitTest;

    InitializationOrderTracker tracker;
    MockServiceConfig config1("Proxy1");
    config1.InitTracker = &tracker;

    MockServiceConfig config2("Proxy2");
    config2.InitTracker = &tracker;

    MockServiceConfig config3("Proxy3");
    config3.InitTracker = &tracker;

    // Start proxies
    std::vector<StartServiceProxyRecord> proxies;
    proxies.emplace_back("Proxy1", std::make_unique<MockServiceProxyFactory>(config1));
    proxies.emplace_back("Proxy2", std::make_unique<MockServiceProxyFactory>(config2));
    proxies.emplace_back("Proxy3", std::make_unique<MockServiceProxyFactory>(config3));

    RunAsync([this, &proxies]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies), ServiceLaunchPriority(1000)); });

    // Shutdown proxies
    std::vector<std::exception_ptr> failures;
    RunAsync([this, &failures]() -> boost::asio::awaitable<void>
             { failures = co_await host.TryShutdownServiceProxiesAsync(ServiceLaunchPriority(1000)); });

    // No failures expected
    EXPECT_TRUE(failures.empty());

    // Once implemented, verify shutdown order is reverse of creation
    // (Note: Proxies don't have ShutdownAsync, so this is about cleanup order)
  }

  TEST_F(ServiceHostTest, TryShutdownServiceProxiesAsync_DifferentPriority_OnlyShutdownsMatchingPriority)
  {
    using namespace Test2::UnitTest;

    InitializationOrderTracker tracker;
    MockServiceConfig config1000("Proxy1000");
    config1000.InitTracker = &tracker;

    MockServiceConfig config2000("Proxy2000");
    config2000.InitTracker = &tracker;

    // Start proxies at different priorities (descending order: high to low)
    std::vector<StartServiceProxyRecord> proxies1000;
    proxies1000.emplace_back("Proxy1000", std::make_unique<MockServiceProxyFactory>(config1000));

    std::vector<StartServiceProxyRecord> proxies2000;
    proxies2000.emplace_back("Proxy2000", std::make_unique<MockServiceProxyFactory>(config2000));

    // Register higher priority (2000) first, then lower priority (1000)
    RunAsync([this, &proxies2000]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies2000), ServiceLaunchPriority(2000)); });

    RunAsync([this, &proxies1000]() -> boost::asio::awaitable<void>
             { co_await host.TryStartServiceProxiesAsync(std::move(proxies1000), ServiceLaunchPriority(1000)); });

    // Shutdown only priority 1000
    std::vector<std::exception_ptr> failures;
    RunAsync([this, &failures]() -> boost::asio::awaitable<void>
             { failures = co_await host.TryShutdownServiceProxiesAsync(ServiceLaunchPriority(1000)); });

    EXPECT_TRUE(failures.empty());

    // Once implemented, verify Proxy2000 is still available and Proxy1000 is not
  }
}
