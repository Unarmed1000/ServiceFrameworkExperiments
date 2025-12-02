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
#include <Test2/Framework/Host/ManagedThreadServiceHost.hpp>
#include <Test2/Framework/Host/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

namespace Test2
{
  // Mock service for testing
  class MockService : public IServiceControl
  {
  private:
    std::string m_name;
    bool m_initShouldFail;
    bool m_shutdownShouldFail;
    mutable bool m_initCalled = false;
    mutable bool m_shutdownCalled = false;

  public:
    explicit MockService(std::string name, bool initShouldFail = false, bool shutdownShouldFail = false)
      : m_name(std::move(name))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
    {
    }

    [[nodiscard]] const std::string& GetName() const noexcept
    {
      return m_name;
    }

    [[nodiscard]] bool WasInitCalled() const noexcept
    {
      return m_initCalled;
    }

    [[nodiscard]] bool WasShutdownCalled() const noexcept
    {
      return m_shutdownCalled;
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      m_initCalled = true;
      if (m_initShouldFail)
      {
        throw std::runtime_error("Init failed for " + m_name);
      }
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      m_shutdownCalled = true;
      if (m_shutdownShouldFail)
      {
        throw std::runtime_error("Shutdown failed for " + m_name);
      }
      co_return ServiceShutdownResult::Success;
    }

    ServiceProcessResult Process() override
    {
      return ServiceProcessResult{};
    }
  };

  struct ITestInterface : public IService
  {
  };

  // Mock factory
  class MockServiceFactory : public IServiceFactory
  {
  private:
    std::string m_serviceName;
    bool m_initShouldFail;
    bool m_shutdownShouldFail;
    const std::type_info* m_interface;
    mutable std::shared_ptr<MockService> m_createdService;

  public:
    explicit MockServiceFactory(std::string serviceName, bool initShouldFail = false, bool shutdownShouldFail = false)
      : m_serviceName(std::move(serviceName))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
      , m_interface(&typeid(ITestInterface))
    {
    }

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      return std::span<const std::type_info>(m_interface, m_interface + 1);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
    {
      m_createdService = std::make_shared<MockService>(m_serviceName, m_initShouldFail, m_shutdownShouldFail);
      return m_createdService;
    }

    [[nodiscard]] std::shared_ptr<MockService> GetCreatedService() const
    {
      return m_createdService;
    }
  };

  // ========================================
  // Phase 3: Empty Service List Handling
  // ========================================

  TEST(ManagedThreadServiceHostTest, EmptyServiceList_Succeeds)
  {
    ManagedThreadServiceProvider provider;
    std::vector<ServiceInstanceInfo> emptyServices;
    EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(emptyServices)), EmptyPriorityGroupException);
  }

  // ========================================
  // Phase 4: Service Initialization Success
  // ========================================

  TEST(ManagedThreadServiceHostTest, SingleService_InitializesSuccessfully)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory = std::make_shared<MockServiceFactory>("TestService");
    auto factoryPtr = factory.get();
    services.emplace_back("TestService", std::move(factory));

    bool completed = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        completed = true;
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(completed);
    auto service = factoryPtr->GetCreatedService();
    ASSERT_NE(service, nullptr);
    EXPECT_TRUE(service->WasInitCalled());
    EXPECT_FALSE(service->WasShutdownCalled());
  }

  TEST(ManagedThreadServiceHostTest, MultipleServices_InitializeInOrder)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory1 = std::make_shared<MockServiceFactory>("Service1");
    auto factory2 = std::make_shared<MockServiceFactory>("Service2");
    auto factory3 = std::make_shared<MockServiceFactory>("Service3");
    auto f1Ptr = factory1.get();
    auto f2Ptr = factory2.get();
    auto f3Ptr = factory3.get();

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));
    services.emplace_back("Service3", std::move(factory3));

    bool completed = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        completed = true;
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(completed);
    EXPECT_TRUE(f1Ptr->GetCreatedService()->WasInitCalled());
    EXPECT_TRUE(f2Ptr->GetCreatedService()->WasInitCalled());
    EXPECT_TRUE(f3Ptr->GetCreatedService()->WasInitCalled());
  }

  // ========================================
  // Phase 5: Init Failure with Rollback
  // ========================================

  TEST(ManagedThreadServiceHostTest, ServiceInitFails_RollsBackSuccessfulServices)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory1 = std::make_shared<MockServiceFactory>("Service1", false);
    auto factory2 = std::make_shared<MockServiceFactory>("Service2", true);    // This will fail
    auto factory3 = std::make_shared<MockServiceFactory>("Service3", false);
    auto f1Ptr = factory1.get();
    auto f2Ptr = factory2.get();
    auto f3Ptr = factory3.get();

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));
    services.emplace_back("Service3", std::move(factory3));

    bool exceptionThrown = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const AggregateException&)
        {
          exceptionThrown = true;
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_TRUE(f1Ptr->GetCreatedService()->WasInitCalled());
    EXPECT_TRUE(f1Ptr->GetCreatedService()->WasShutdownCalled());     // Should be rolled back
    EXPECT_TRUE(f2Ptr->GetCreatedService()->WasInitCalled());         // Failed during init
    EXPECT_FALSE(f2Ptr->GetCreatedService()->WasShutdownCalled());    // Never successfully initialized
  }

  TEST(ManagedThreadServiceHostTest, FirstServiceFails_NoRollbackNeeded)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory = std::make_shared<MockServiceFactory>("Service1", true);
    auto factoryPtr = factory.get();
    services.emplace_back("Service1", std::move(factory));

    bool exceptionThrown = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const AggregateException&)
        {
          exceptionThrown = true;
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_TRUE(factoryPtr->GetCreatedService()->WasInitCalled());
    EXPECT_FALSE(factoryPtr->GetCreatedService()->WasShutdownCalled());
  }

  // ========================================
  // Phase 6: Multiple Failures
  // ========================================

  TEST(ManagedThreadServiceHostTest, MultipleServicesFail_AggregatesAllExceptions)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory1 = std::make_shared<MockServiceFactory>("Service1", true);    // Fails
    auto factory2 = std::make_shared<MockServiceFactory>("Service2", false);
    auto factory3 = std::make_shared<MockServiceFactory>("Service3", true);    // Fails

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));
    services.emplace_back("Service3", std::move(factory3));

    AggregateException caughtException;
    bool exceptionThrown = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const AggregateException& ex)
        {
          caughtException = ex;
          exceptionThrown = true;
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_GE(caughtException.GetExceptions().size(), 2);    // At least 2 init failures
  }

  TEST(ManagedThreadServiceHostTest, RollbackFailure_IncludedInAggregateException)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory1 = std::make_shared<MockServiceFactory>("Service1", false, true);    // Shutdown fails
    auto factory2 = std::make_shared<MockServiceFactory>("Service2", true);           // Init fails

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));

    AggregateException caughtException;
    bool exceptionThrown = false;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const AggregateException& ex)
        {
          caughtException = ex;
          exceptionThrown = true;
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_GE(caughtException.GetExceptions().size(), 2);    // Init failure + shutdown failure
  }
}
