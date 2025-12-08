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

  // ========================================
  // Phase 4: Service Initialization Success
  // ========================================

  TEST(ManagedThreadServiceHostTest, SingleService_InitializesSuccessfully)
  {
    ManagedThreadServiceHost host;

    auto tracker = std::make_shared<ServiceLifecycleTracker>();
    auto factory = std::make_unique<MockServiceFactory>("TestService", tracker);

    bool completed = false;
    std::exception_ptr exceptionPtr;

    // Start the host's io_context on a separate thread (as it would be in production)
    std::thread hostThread(
      [&host, &completed, &exceptionPtr, factory = std::move(factory)]() mutable
      {
        spdlog::info("Host thread started");

        // Post the coroutine work to the io_context
        boost::asio::co_spawn(
          host.GetIoContext(),
          [&]() -> boost::asio::awaitable<void>
          {
            spdlog::info("Coroutine started");
            try
            {
              std::vector<StartServiceRecord> services;
              services.emplace_back("TestService", std::move(factory));

              spdlog::info("Calling TryStartServicesAsync");
              co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000));
              spdlog::info("TryStartServicesAsync completed");
              completed = true;
            }
            catch (...)
            {
              spdlog::error("Exception in coroutine");
              exceptionPtr = std::current_exception();
            }
            spdlog::info("Coroutine ending");
          },
          [&host](std::exception_ptr e)
          {
            if (e)
            {
              spdlog::error("Unhandled exception in coroutine");
            }
            // Stop the io_context when coroutine completes
            host.GetIoContext().stop();
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
    EXPECT_TRUE(tracker->initCalled);
    EXPECT_FALSE(tracker->shutdownCalled);
  }
  TEST(ManagedThreadServiceHostTest, MultipleServices_InitializeInOrder)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();
    auto factory1 = std::make_unique<MockServiceFactory>("Service1", tracker1);
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", tracker2);
    auto factory3 = std::make_unique<MockServiceFactory>("Service3", tracker3);

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
      [&host](std::exception_ptr e)
      {
        if (e)
        {
          std::rethrow_exception(e);
        }
        host.GetIoContext().stop();
      });

    host.GetIoContext().run();

    EXPECT_TRUE(completed);
    EXPECT_TRUE(tracker1->initCalled);
    EXPECT_TRUE(tracker2->initCalled);
    EXPECT_TRUE(tracker3->initCalled);
  }

  // ========================================
  // Phase 5: Init Failure with Rollback
  // ========================================

  TEST(ManagedThreadServiceHostTest, ServiceInitFails_RollsBackSuccessfulServices)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();
    auto factory1 = std::make_unique<MockServiceFactory>("Service1", tracker1, false);
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", tracker2, true);    // This will fail
    auto factory3 = std::make_unique<MockServiceFactory>("Service3", tracker3, false);

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
      [&host](std::exception_ptr e)
      {
        if (e)
        {
          std::rethrow_exception(e);
        }
        host.GetIoContext().stop();
      });

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_TRUE(tracker1->initCalled);
    EXPECT_TRUE(tracker1->shutdownCalled);     // Should be rolled back
    EXPECT_TRUE(tracker2->initCalled);         // Failed during init
    EXPECT_FALSE(tracker2->shutdownCalled);    // Never successfully initialized
  }

  TEST(ManagedThreadServiceHostTest, FirstServiceFails_NoRollbackNeeded)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto tracker = std::make_shared<ServiceLifecycleTracker>();
    auto factory = std::make_unique<MockServiceFactory>("Service1", tracker, true);
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
      [&host](std::exception_ptr e)
      {
        if (e)
        {
          std::rethrow_exception(e);
        }
        host.GetIoContext().stop();
      });

    host.GetIoContext().run();

    EXPECT_TRUE(exceptionThrown);
    EXPECT_TRUE(tracker->initCalled);
    EXPECT_FALSE(tracker->shutdownCalled);
  }

  // ========================================
  // Phase 6: Multiple Failures
  // ========================================

  TEST(ManagedThreadServiceHostTest, MultipleServicesFail_AggregatesAllExceptions)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker3 = std::make_shared<ServiceLifecycleTracker>();
    auto factory1 = std::make_unique<MockServiceFactory>("Service1", tracker1, true);    // Fails
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", tracker2, false);
    auto factory3 = std::make_unique<MockServiceFactory>("Service3", tracker3, true);    // Fails

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
      [&host](std::exception_ptr e)
      {
        if (e)
        {
          std::rethrow_exception(e);
        }
        host.GetIoContext().stop();
      });

    host.GetIoContext().run();

    ASSERT_TRUE(caughtException.has_value());
    EXPECT_GE(caughtException->GetInnerExceptions().size(), 2);    // At least 2 init failures
  }

  TEST(ManagedThreadServiceHostTest, RollbackFailure_IncludedInAggregateException)
  {
    ManagedThreadServiceHost host;
    std::vector<StartServiceRecord> services;

    auto tracker1 = std::make_shared<ServiceLifecycleTracker>();
    auto tracker2 = std::make_shared<ServiceLifecycleTracker>();
    auto factory1 = std::make_unique<MockServiceFactory>("Service1", tracker1, false, true);    // Shutdown fails
    auto factory2 = std::make_unique<MockServiceFactory>("Service2", tracker2, true);           // Init fails

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
      [&host](std::exception_ptr e)
      {
        if (e)
        {
          std::rethrow_exception(e);
        }
        host.GetIoContext().stop();
      });

    host.GetIoContext().run();

    ASSERT_TRUE(caughtException.has_value());
    EXPECT_GE(caughtException->GetInnerExceptions().size(), 2);    // Init failure + shutdown failure
  }
}
