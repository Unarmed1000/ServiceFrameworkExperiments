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
#include <boost/asio/post.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>

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
      spdlog::info("MockService::InitAsync called for {}", m_name);
      m_initCalled = true;
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
    mutable std::shared_ptr<MockService> m_createdService;

  public:
    explicit MockServiceFactory(std::string serviceName, bool initShouldFail = false, bool shutdownShouldFail = false)
      : m_serviceName(std::move(serviceName))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
    {
    }

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      static const std::type_info* interfaces[] = {&typeid(ITestInterface)};
      return std::span<const std::type_info>(reinterpret_cast<const std::type_info*>(interfaces), 1);
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

    auto factory = std::make_unique<MockServiceFactory>("TestService");
    auto factoryPtr = factory.get();

    // Move factory into a shared location that outlives the coroutine
    auto factoryShared = std::make_shared<std::unique_ptr<MockServiceFactory>>(std::move(factory));

    bool completed = false;
    std::exception_ptr exceptionPtr;

    // Start the host's io_context on a separate thread (as it would be in production)
    std::thread hostThread(
      [&host, factoryShared, &completed, &exceptionPtr]()
      {
        spdlog::info("Host thread started");
        // Post the coroutine to run on the host's thread
        boost::asio::post(host.GetIoContext(),
                          [&]()
                          {
                            spdlog::info("Post handler executing, spawning coroutine");
                            boost::asio::co_spawn(
                              host.GetIoContext(),
                              [&]() -> boost::asio::awaitable<void>
                              {
                                spdlog::info("Coroutine started");
                                try
                                {
                                  std::vector<StartServiceRecord> services;
                                  services.emplace_back("TestService", std::move(*factoryShared));

                                  spdlog::info("Calling TryStartServicesAsync");
                                  co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
                                  spdlog::info("TryStartServicesAsync completed");
                                  completed = true;

                                  // Stop the io_context so it exits
                                  host.GetIoContext().stop();
                                }
                                catch (...)
                                {
                                  spdlog::error("Exception in coroutine");
                                  exceptionPtr = std::current_exception();
                                  host.GetIoContext().stop();
                                }
                                spdlog::info("Coroutine ending");
                              },
                              boost::asio::detached);
                            spdlog::info("Post handler completed");
                          });

        spdlog::info("Calling io_context.run()");
        host.GetIoContext().run();
        spdlog::info("io_context.run() completed");
      });

    spdlog::info("Waiting for host thread to join");
    hostThread.join();
    spdlog::info("Host thread joined");

    if (exceptionPtr)
    {
      std::rethrow_exception(exceptionPtr);
    }

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

    auto factory1 = std::make_unique<MockServiceFactory>("Service1");
    auto factory2 = std::make_unique<MockServiceFactory>("Service2");
    auto factory3 = std::make_unique<MockServiceFactory>("Service3");
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

    auto factory1 = std::make_unique<MockServiceFactory>("Service1", false);
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", true);    // This will fail
    auto factory3 = std::make_unique<MockServiceFactory>("Service3", false);
    auto f1Ptr = factory1.get();
    auto f2Ptr = factory2.get();

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
        catch (const Common::AggregateException&)
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

    auto factory = std::make_unique<MockServiceFactory>("Service1", true);
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
        catch (const Common::AggregateException&)
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

    auto factory1 = std::make_unique<MockServiceFactory>("Service1", true);    // Fails
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", false);
    auto factory3 = std::make_unique<MockServiceFactory>("Service3", true);    // Fails

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));
    services.emplace_back("Service3", std::move(factory3));

    std::optional<Common::AggregateException> caughtException;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const Common::AggregateException& ex)
        {
          caughtException.emplace(ex);
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    ASSERT_TRUE(caughtException.has_value());
    EXPECT_GE(caughtException->GetInnerExceptions().size(), 2);    // At least 2 init failures
  }

  TEST(ManagedThreadServiceHostTest, RollbackFailure_IncludedInAggregateException)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto factory1 = std::make_unique<MockServiceFactory>("Service1", false, true);    // Shutdown fails
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", true);           // Init fails

    services.emplace_back("Service1", std::move(factory1));
    services.emplace_back("Service2", std::move(factory2));

    std::optional<Common::AggregateException> caughtException;
    boost::asio::co_spawn(
      host.GetIoContext(),
      [&]() -> boost::asio::awaitable<void>
      {
        try
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
        }
        catch (const Common::AggregateException& ex)
        {
          caughtException.emplace(ex);
        }
      },
      boost::asio::detached);

    host.GetIoContext().run();

    ASSERT_TRUE(caughtException.has_value());
    EXPECT_GE(caughtException->GetInnerExceptions().size(), 2);    // Init failure + shutdown failure
  }
}
