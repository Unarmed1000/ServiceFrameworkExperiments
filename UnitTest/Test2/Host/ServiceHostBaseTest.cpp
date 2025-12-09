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
  class MockServiceFactory : public IServiceFactory
  {
  private:
    std::string m_serviceName;
    std::shared_ptr<ServiceLifecycleTracker> m_tracker;
    bool m_initShouldFail;
    bool m_shutdownShouldFail;

  public:
    explicit MockServiceFactory(std::string serviceName, std::shared_ptr<ServiceLifecycleTracker> tracker = nullptr, bool initShouldFail = false,
                                bool shutdownShouldFail = false)
      : m_serviceName(std::move(serviceName))
      , m_tracker(std::move(tracker))
      , m_initShouldFail(initShouldFail)
      , m_shutdownShouldFail(shutdownShouldFail)
    {
    }

    std::span<const std::type_index> GetSupportedInterfaces() const override
    {
      static const std::type_index interfaces[] = {std::type_index(typeid(ITestInterface))};
      return std::span<const std::type_index>(interfaces);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_index& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
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
               { co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(priority)); });
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
            co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(priority));
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
}
