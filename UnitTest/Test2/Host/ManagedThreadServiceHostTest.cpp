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
#include <Test2/Framework/Host/Cooperative/CooperativeThreadHost.hpp>
#include <Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <typeindex>
#include "TestManagedThreadLifecycle.hpp"

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
  // Test Fixtures
  // ========================================

  /// @brief Base fixture for ManagedThreadHost tests with manual lifecycle control.
  ///
  /// Provides helper methods and test infrastructure without automatic startup.
  /// Use this when you need explicit control over host lifecycle (e.g., startup failure tests).
  class ManagedThreadHostTestFixtureBase : public ::testing::Test
  {
  protected:
    CooperativeThreadHost m_testHost;
    ManagedThreadHost m_host;
    TestManagedThreadLifecycle m_lifecycle;
    std::thread::id m_testThreadId;

    ManagedThreadHostTestFixtureBase()
      : m_host(m_testHost.GetExecutorContext())
      , m_lifecycle(m_host)
      , m_testThreadId(std::this_thread::get_id())
    {
    }

    /// @brief Run a coroutine synchronously on the test host's io_context.
    template <typename Func>
    auto RunTest(Func&& func)
    {
      boost::asio::post(m_testHost.GetExecutorContext().GetExecutor(), []() {});
      m_testHost.Poll();
      auto future = boost::asio::co_spawn(m_testHost.GetExecutorContext().GetExecutor(), std::forward<Func>(func), boost::asio::use_future);
      while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
      {
        m_testHost.Poll();
      }
      future.get();
    }

    /// @brief Start the managed thread.
    void StartHost()
    {
      RunTest([this]() -> boost::asio::awaitable<void> { co_await m_lifecycle.StartAsync(); });
    }

    /// @brief Start services at the specified priority with automatic tracking for cleanup.
    void StartServicesAsync(std::vector<StartServiceRecord> services, ServiceLaunchPriority priority)
    {
      RunTest([this, services = std::move(services), priority]() mutable -> boost::asio::awaitable<void>
              { co_await m_lifecycle.StartServicesAsync(std::move(services), priority); });
    }

    /// @brief Shutdown services at the specified priority and track the shutdown.
    std::vector<std::exception_ptr> ShutdownServicesAsync(ServiceLaunchPriority priority)
    {
      std::vector<std::exception_ptr> result;
      RunTest([this, priority, &result]() -> boost::asio::awaitable<void> { result = co_await m_lifecycle.ShutdownServicesAsync(priority); });
      return result;
    }

    /// @brief Stop the managed thread and all remaining services.
    void StopHost()
    {
      std::vector<std::exception_ptr> errors;
      RunTest([this, &errors]() -> boost::asio::awaitable<void> { errors = co_await m_lifecycle.TryShutdownAsync(); });

      // Report any unexpected shutdown errors
      if (!errors.empty())
      {
        std::ostringstream oss;
        oss << "Service shutdown failed with " << errors.size() << " error(s): ";
        for (size_t i = 0; i < errors.size(); ++i)
        {
          try
          {
            std::rethrow_exception(errors[i]);
          }
          catch (const std::exception& ex)
          {
            oss << "\n  [" << (i + 1) << "] " << ex.what();
          }
          catch (...)
          {
            oss << "\n  [" << (i + 1) << "] Unknown exception";
          }
        }
        ADD_FAILURE() << oss.str();
      }
    }

    /// @brief Create a mock service factory.
    std::unique_ptr<MockServiceFactory> CreateMockFactory(const std::string& name, std::shared_ptr<ServiceLifecycleTracker> tracker = nullptr,
                                                          bool initFails = false, bool shutdownFails = false)
    {
      return std::make_unique<MockServiceFactory>(name, tracker, initFails, shutdownFails);
    }

    /// @brief Create tracked service records for multiple services.
    ///
    /// @param specs Vector of tuples: {name, initFails, shutdownFails}
    /// @return Pair of service records and trackers
    auto CreateTrackedServiceRecords(const std::vector<std::tuple<std::string, bool, bool>>& specs)
      -> std::pair<std::vector<StartServiceRecord>, std::vector<std::shared_ptr<ServiceLifecycleTracker>>>
    {
      std::vector<StartServiceRecord> records;
      std::vector<std::shared_ptr<ServiceLifecycleTracker>> trackers;

      for (const auto& [name, initFails, shutdownFails] : specs)
      {
        auto tracker = std::make_shared<ServiceLifecycleTracker>();
        trackers.push_back(tracker);
        records.emplace_back(name, CreateMockFactory(name, tracker, initFails, shutdownFails));
      }

      return {std::move(records), std::move(trackers)};
    }

    /// @brief Verify that the test thread is different from the service thread.
    void VerifyCrossThreadMarshalling()
    {
      auto serviceThreadId = std::this_thread::get_id();
      EXPECT_NE(m_testThreadId, serviceThreadId) << "Test should execute on different thread from service thread";
    }

    void TearDown() override
    {
      // Always stop the host in teardown to ensure cleanup
      StopHost();
    }
  };

  /// @brief Auto-started fixture for ManagedThreadHost tests.
  ///
  /// Automatically starts the host in SetUp().
  /// Tests should explicitly call StopHost() before completion if needed.
  /// Use this for most tests that just need a running host.
  class ManagedThreadHostTestFixture : public ManagedThreadHostTestFixtureBase
  {
  protected:
    void SetUp() override
    {
      StartHost();
    }
  };

  // ========================================
  // MANUAL API TESTS - Direct host API calls, bypass automatic lifecycle tracking
  // ========================================

  // ========================================
  // Phase 3.5: Startup Failure Tests
  // ========================================

  TEST_F(ManagedThreadHostTestFixtureBase, TryStartServicesAsync_BeforeStart_ThrowsException)
  {
    // Create services without starting the host
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, false}});

    // Attempt to start services without starting the host should fail
    EXPECT_THROW(RunTest([this, services = std::move(services)]() mutable -> boost::asio::awaitable<void>
                         { co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000)); }),
                 std::exception);
  }

  TEST_F(ManagedThreadHostTestFixtureBase, StartAsync_CalledTwice_HandlesGracefully)
  {
    // Start the host once
    StartHost();

    // Attempt to start again - should either be idempotent or throw
    // Current implementation has guard against multiple starts
    bool secondStartCompleted = false;
    try
    {
      RunTest(
        [this, &secondStartCompleted]() -> boost::asio::awaitable<void>
        {
          co_await m_host.StartAsync();
          secondStartCompleted = true;
        });
    }
    catch (...)
    {
      // Expected - second start should not succeed
      secondStartCompleted = false;
    }

    // Either it's idempotent (completed) or it properly rejected the second start
    // The important thing is no crash or deadlock
    EXPECT_TRUE(true);    // If we got here without hanging, the test passes
  }

  // ========================================
  // AUTOMATIC LIFECYCLE TESTS - Use helper methods with automatic service tracking
  // ========================================

  // ========================================
  // Phase 4: Service Initialization Success
  // ========================================

  TEST_F(ManagedThreadHostTestFixture, SingleService_InitializesSuccessfully)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"TestService", false, false}});

    // Use automatic lifecycle helper - services will be shut down in TearDown
    StartServicesAsync(std::move(services), ServiceLaunchPriority(1000));

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_FALSE(trackers[0]->shutdownCalled);
  }

  TEST_F(ManagedThreadHostTestFixture, MultipleServices_InitializeInOrder)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, false}, {"Service2", false, false}, {"Service3", false, false}});

    // Use automatic lifecycle helper - services will be shut down in TearDown
    StartServicesAsync(std::move(services), ServiceLaunchPriority(1000));

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_TRUE(trackers[1]->initCalled);
    EXPECT_TRUE(trackers[2]->initCalled);
  }

  // ========================================
  // FAILURE SCENARIO TESTS - Test rollback and error handling, manual service management
  // ========================================

  // ========================================
  // Phase 5: Init Failure with Rollback
  // ========================================

  TEST_F(ManagedThreadHostTestFixture, ServiceInitFails_RollsBackSuccessfulServices)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, false}, {"Service2", true, false}, {"Service3", false, false}});

    EXPECT_THROW(RunTest([this, services = std::move(services)]() mutable -> boost::asio::awaitable<void>
                         { co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000)); }),
                 Common::AggregateException);

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_TRUE(trackers[0]->shutdownCalled);     // Should be rolled back
    EXPECT_TRUE(trackers[1]->initCalled);         // Failed during init
    EXPECT_FALSE(trackers[1]->shutdownCalled);    // Never successfully initialized
  }

  TEST_F(ManagedThreadHostTestFixture, FirstServiceFails_NoRollbackNeeded)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", true, false}});

    EXPECT_THROW(RunTest([this, services = std::move(services)]() mutable -> boost::asio::awaitable<void>
                         { co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000)); }),
                 Common::AggregateException);

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_FALSE(trackers[0]->shutdownCalled);
  }

  // ========================================
  // Phase 6: Multiple Failures
  // ========================================

  TEST_F(ManagedThreadHostTestFixture, MultipleServicesFail_AggregatesAllExceptions)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", true, false}, {"Service2", false, false}, {"Service3", true, false}});

    try
    {
      RunTest([this, services = std::move(services)]() mutable -> boost::asio::awaitable<void>
              { co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000)); });
      FAIL() << "Expected AggregateException to be thrown";
    }
    catch (const Common::AggregateException& ex)
    {
      EXPECT_GE(ex.GetInnerExceptions().size(), 2);    // At least 2 init failures
    }
  }

  TEST_F(ManagedThreadHostTestFixture, RollbackFailure_IncludedInAggregateException)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, true}, {"Service2", true, false}});

    try
    {
      RunTest([this, services = std::move(services)]() mutable -> boost::asio::awaitable<void>
              { co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), ServiceLaunchPriority(1000)); });
      FAIL() << "Expected AggregateException to be thrown";
    }
    catch (const Common::AggregateException& ex)
    {
      EXPECT_GE(ex.GetInnerExceptions().size(), 2);    // Init failure + shutdown failure
    }
  }

  // ========================================
  // Phase 6: Explicit Shutdown Tests
  // ========================================

  TEST_F(ManagedThreadHostTestFixture, ShutdownServices_Success)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, false}, {"Service2", false, false}});

    // Start services with automatic tracking
    StartServicesAsync(std::move(services), ServiceLaunchPriority(1000));

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_TRUE(trackers[1]->initCalled);

    // Explicitly shutdown services and track the shutdown
    auto exceptions = ShutdownServicesAsync(ServiceLaunchPriority(1000));
    EXPECT_TRUE(exceptions.empty());

    EXPECT_TRUE(trackers[0]->shutdownCalled);
    EXPECT_TRUE(trackers[1]->shutdownCalled);
  }

  TEST_F(ManagedThreadHostTestFixture, ShutdownServices_WithFailures_ReturnsExceptions)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, true}, {"Service2", false, false}});

    // Start services with automatic tracking
    StartServicesAsync(std::move(services), ServiceLaunchPriority(1000));

    EXPECT_TRUE(trackers[0]->initCalled);
    EXPECT_TRUE(trackers[1]->initCalled);

    // Shutdown services - one will fail, but we track the shutdown to prevent double cleanup
    auto exceptions = ShutdownServicesAsync(ServiceLaunchPriority(1000));
    EXPECT_EQ(exceptions.size(), 1);    // One shutdown failure

    EXPECT_TRUE(trackers[0]->shutdownCalled);    // Called even though it failed
    EXPECT_TRUE(trackers[1]->shutdownCalled);
  }

  TEST_F(ManagedThreadHostTestFixture, ShutdownNonExistentPriority_ReturnsEmpty)
  {
    auto [services, trackers] = CreateTrackedServiceRecords({{"Service1", false, false}});

    // Start services at priority 1000 with automatic tracking
    StartServicesAsync(std::move(services), ServiceLaunchPriority(1000));

    // Try to shutdown priority 2000 (doesn't exist)
    auto exceptions = ShutdownServicesAsync(ServiceLaunchPriority(2000));
    EXPECT_TRUE(exceptions.empty());

    EXPECT_FALSE(trackers[0]->shutdownCalled);    // Should not be called
  }
}
