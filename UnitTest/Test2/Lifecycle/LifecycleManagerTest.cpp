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
#include <Test2/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <typeindex>
#include <vector>

namespace Test2
{
  // ============================================================================
  // Shared initialization order tracker
  // ============================================================================

  class InitializationOrderTracker
  {
  public:
    std::vector<std::string> Order;
    std::mutex Mutex;

    void RecordInit(const std::string& serviceName)
    {
      std::lock_guard<std::mutex> lock(Mutex);
      Order.push_back(serviceName);
    }

    void Clear()
    {
      std::lock_guard<std::mutex> lock(Mutex);
      Order.clear();
    }
  };

  // ============================================================================
  // Mock Service for Testing
  // ============================================================================

  class MockLifecycleService : public IServiceControl
  {
  private:
    ProcessResult m_processResult;
    std::atomic<int> m_processCallCount{0};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};
    std::string m_name;
    InitializationOrderTracker* m_tracker{nullptr};
    std::thread::id m_initThreadId;

  public:
    explicit MockLifecycleService(ProcessResult processResult = ProcessResult::NoSleepLimit())
      : m_processResult(processResult)
    {
    }

    MockLifecycleService(const std::string& name, InitializationOrderTracker* tracker, ProcessResult processResult = ProcessResult::NoSleepLimit())
      : m_processResult(processResult)
      , m_name(name)
      , m_tracker(tracker)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      m_initThreadId = std::this_thread::get_id();
      m_initialized = true;
      if (m_tracker)
      {
        m_tracker->RecordInit(m_name);
      }
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      m_shutdown = true;
      co_return ServiceShutdownResult::Success;
    }

    ProcessResult Process() override
    {
      ++m_processCallCount;
      return m_processResult;
    }

    int GetProcessCallCount() const noexcept
    {
      return m_processCallCount.load();
    }

    bool IsInitialized() const noexcept
    {
      return m_initialized.load();
    }

    bool IsShutdown() const noexcept
    {
      return m_shutdown.load();
    }

    void SetProcessResult(ProcessResult result)
    {
      m_processResult = result;
    }

    std::thread::id GetInitThreadId() const noexcept
    {
      return m_initThreadId;
    }
  };

  struct ITestInterface : public IService
  {
  };

  // Mock factory
  class MockLifecycleServiceFactory : public IServiceFactory
  {
  private:
    std::shared_ptr<MockLifecycleService> m_service;

  public:
    explicit MockLifecycleServiceFactory(std::shared_ptr<MockLifecycleService> service)
      : m_service(std::move(service))
    {
    }

    std::span<const std::type_index> GetSupportedInterfaces() const override
    {
      static const std::type_index interfaces[] = {std::type_index(typeid(ITestInterface))};
      return std::span<const std::type_index>(interfaces);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_index& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
    {
      return m_service;
    }
  };

  // ============================================================================
  // Phase 1: Basic Construction and Destruction Tests
  // ============================================================================

  TEST(LifecycleManager, Construction_WithEmptyRegistrations_Succeeds)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));
    // Should construct without throwing
    SUCCEED();
  }

  TEST(LifecycleManager, Destruction_Succeeds)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    auto manager = std::make_unique<LifecycleManager>(config, std::move(registrations));
    manager.reset();
    // Should destruct without throwing
    SUCCEED();
  }

  TEST(LifecycleManager, GetMainHost_ReturnsValidHost)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));

    // Should return a valid reference
    auto& host = manager.GetMainHost();
    (void)host;
    SUCCEED();
  }

  TEST(LifecycleManager, Poll_WithNoServices_ReturnsZero)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));

    auto count = manager.Poll();
    EXPECT_EQ(count, 0u);
  }

  TEST(LifecycleManager, Update_WithNoServices_ReturnsNoSleepLimit)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));

    auto result = manager.Update();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  // ============================================================================
  // Phase 2: Single Thread Group (Main Only) Tests
  // ============================================================================

  // Helper to run async operations synchronously using polling
  void RunAsyncWithPolling(LifecycleManager& manager, std::function<boost::asio::awaitable<void>()> asyncOp)
  {
    bool done = false;
    boost::asio::co_spawn(
      manager.GetMainHost().GetIoContext(),
      [&asyncOp, &done]() -> boost::asio::awaitable<void>
      {
        co_await asyncOp();
        done = true;
      },
      boost::asio::detached);

    while (!done)
    {
      manager.Poll();
    }
  }

  TEST(LifecycleManager, StartServicesAsync_WithEmptyRegistrations_Succeeds)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Should complete without throwing
    SUCCEED();
  }

  TEST(LifecycleManager, StartServicesAsync_SingleService_MainThreadGroup_ServiceInitialized)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(service->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(service->IsInitialized());
  }

  TEST(LifecycleManager, StartServicesAsync_SingleService_MainThreadGroup_ProcessCalled)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_EQ(service->GetProcessCallCount(), 0);

    manager.Update();

    EXPECT_EQ(service->GetProcessCallCount(), 1);
  }

  TEST(LifecycleManager, StartServicesAsync_MultipleServices_SamePriority_MainThreadGroup_AllInitialized)
  {
    auto service1 = std::make_shared<MockLifecycleService>();
    auto service2 = std::make_shared<MockLifecycleService>();
    auto factory1 = std::make_unique<MockLifecycleServiceFactory>(service1);
    auto factory2 = std::make_unique<MockLifecycleServiceFactory>(service2);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory1), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(factory2), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(service1->IsInitialized());
    EXPECT_FALSE(service2->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(service1->IsInitialized());
    EXPECT_TRUE(service2->IsInitialized());
  }

  // ============================================================================
  // Phase 3: Multiple Priorities Tests
  // ============================================================================

  TEST(LifecycleManager, StartServicesAsync_TwoPriorities_HigherPriorityInitializedFirst)
  {
    InitializationOrderTracker tracker;

    auto highPriorityService = std::make_shared<MockLifecycleService>("HighPriority", &tracker);
    auto lowPriorityService = std::make_shared<MockLifecycleService>("LowPriority", &tracker);
    auto highFactory = std::make_unique<MockLifecycleServiceFactory>(highPriorityService);
    auto lowFactory = std::make_unique<MockLifecycleServiceFactory>(lowPriorityService);

    std::vector<ServiceRegistrationRecord> registrations;
    // Register in reverse order to ensure priority sorting works
    registrations.emplace_back(std::move(lowFactory), ServiceLaunchPriority(100), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(highFactory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // High priority (1000) should be initialized before low priority (100)
    ASSERT_EQ(tracker.Order.size(), 2u);
    EXPECT_EQ(tracker.Order[0], "HighPriority");
    EXPECT_EQ(tracker.Order[1], "LowPriority");
  }

  TEST(LifecycleManager, StartServicesAsync_ThreePriorities_InitializedHighestToLowest)
  {
    InitializationOrderTracker tracker;

    auto highService = std::make_shared<MockLifecycleService>("High", &tracker);
    auto medService = std::make_shared<MockLifecycleService>("Medium", &tracker);
    auto lowService = std::make_shared<MockLifecycleService>("Low", &tracker);
    auto highFactory = std::make_unique<MockLifecycleServiceFactory>(highService);
    auto medFactory = std::make_unique<MockLifecycleServiceFactory>(medService);
    auto lowFactory = std::make_unique<MockLifecycleServiceFactory>(lowService);

    std::vector<ServiceRegistrationRecord> registrations;
    // Register in scrambled order
    registrations.emplace_back(std::move(medFactory), ServiceLaunchPriority(500), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(lowFactory), ServiceLaunchPriority(100), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(highFactory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Should be initialized highest to lowest: 1000 -> 500 -> 100
    ASSERT_EQ(tracker.Order.size(), 3u);
    EXPECT_EQ(tracker.Order[0], "High");
    EXPECT_EQ(tracker.Order[1], "Medium");
    EXPECT_EQ(tracker.Order[2], "Low");
  }

  TEST(LifecycleManager, StartServicesAsync_MultipleServicesPerPriority_GroupedByPriority)
  {
    InitializationOrderTracker tracker;

    auto high1 = std::make_shared<MockLifecycleService>("High1", &tracker);
    auto high2 = std::make_shared<MockLifecycleService>("High2", &tracker);
    auto low1 = std::make_shared<MockLifecycleService>("Low1", &tracker);
    auto low2 = std::make_shared<MockLifecycleService>("Low2", &tracker);

    std::vector<ServiceRegistrationRecord> registrations;
    // Interleave high and low priority registrations
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(low1), ServiceLaunchPriority(100), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(high1), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(low2), ServiceLaunchPriority(100), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(high2), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // All high priority services should come before all low priority services
    ASSERT_EQ(tracker.Order.size(), 4u);
    // First two should be High1 and High2 (order within same priority may vary)
    std::vector<std::string> highPriorityNames(tracker.Order.begin(), tracker.Order.begin() + 2);
    std::vector<std::string> lowPriorityNames(tracker.Order.begin() + 2, tracker.Order.end());

    EXPECT_TRUE(std::find(highPriorityNames.begin(), highPriorityNames.end(), "High1") != highPriorityNames.end());
    EXPECT_TRUE(std::find(highPriorityNames.begin(), highPriorityNames.end(), "High2") != highPriorityNames.end());
    EXPECT_TRUE(std::find(lowPriorityNames.begin(), lowPriorityNames.end(), "Low1") != lowPriorityNames.end());
    EXPECT_TRUE(std::find(lowPriorityNames.begin(), lowPriorityNames.end(), "Low2") != lowPriorityNames.end());
  }

  // ============================================================================
  // Phase 4: Multiple Thread Groups Tests
  // ============================================================================

  TEST(LifecycleManager, StartServicesAsync_SingleService_NonMainThreadGroup_ServiceInitialized)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    ServiceThreadGroupId workerThreadGroup{1};    // Non-main thread group

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(service->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(service->IsInitialized());
  }

  TEST(LifecycleManager, StartServicesAsync_SingleService_NonMainThreadGroup_RunsOnDifferentThread)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    ServiceThreadGroupId workerThreadGroup{1};
    std::thread::id mainThreadId = std::this_thread::get_id();

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    ASSERT_TRUE(service->IsInitialized());
    // Non-main thread group services should NOT run on the main thread
    EXPECT_NE(service->GetInitThreadId(), mainThreadId);
  }

  TEST(LifecycleManager, StartServicesAsync_MainThreadGroup_RunsOnMainThread)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    std::thread::id mainThreadId = std::this_thread::get_id();

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    ASSERT_TRUE(service->IsInitialized());
    // Main thread group services SHOULD run on the main thread
    EXPECT_EQ(service->GetInitThreadId(), mainThreadId);
  }

  TEST(LifecycleManager, StartServicesAsync_MultipleServices_SameNonMainThreadGroup_AllInitialized)
  {
    auto service1 = std::make_shared<MockLifecycleService>();
    auto service2 = std::make_shared<MockLifecycleService>();
    auto factory1 = std::make_unique<MockLifecycleServiceFactory>(service1);
    auto factory2 = std::make_unique<MockLifecycleServiceFactory>(service2);

    ServiceThreadGroupId workerThreadGroup{1};

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory1), ServiceLaunchPriority(1000), workerThreadGroup);
    registrations.emplace_back(std::move(factory2), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(service1->IsInitialized());
    EXPECT_FALSE(service2->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(service1->IsInitialized());
    EXPECT_TRUE(service2->IsInitialized());
  }

  TEST(LifecycleManager, StartServicesAsync_MultipleServices_SameNonMainThreadGroup_ShareSameThread)
  {
    auto service1 = std::make_shared<MockLifecycleService>();
    auto service2 = std::make_shared<MockLifecycleService>();
    auto factory1 = std::make_unique<MockLifecycleServiceFactory>(service1);
    auto factory2 = std::make_unique<MockLifecycleServiceFactory>(service2);

    ServiceThreadGroupId workerThreadGroup{1};

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory1), ServiceLaunchPriority(1000), workerThreadGroup);
    registrations.emplace_back(std::move(factory2), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    ASSERT_TRUE(service1->IsInitialized());
    ASSERT_TRUE(service2->IsInitialized());
    // Services in the same thread group should share the same thread
    EXPECT_EQ(service1->GetInitThreadId(), service2->GetInitThreadId());
  }

  TEST(LifecycleManager, StartServicesAsync_MixedThreadGroups_MainAndWorker_AllInitialized)
  {
    auto mainService = std::make_shared<MockLifecycleService>();
    auto workerService = std::make_shared<MockLifecycleService>();
    auto mainFactory = std::make_unique<MockLifecycleServiceFactory>(mainService);
    auto workerFactory = std::make_unique<MockLifecycleServiceFactory>(workerService);

    ServiceThreadGroupId workerThreadGroup{1};

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(mainFactory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(workerFactory), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(mainService->IsInitialized());
    EXPECT_FALSE(workerService->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(mainService->IsInitialized());
    EXPECT_TRUE(workerService->IsInitialized());
  }

  TEST(LifecycleManager, StartServicesAsync_MixedThreadGroups_MainAndWorker_RunOnDifferentThreads)
  {
    auto mainService = std::make_shared<MockLifecycleService>();
    auto workerService = std::make_shared<MockLifecycleService>();
    auto mainFactory = std::make_unique<MockLifecycleServiceFactory>(mainService);
    auto workerFactory = std::make_unique<MockLifecycleServiceFactory>(workerService);

    ServiceThreadGroupId workerThreadGroup{1};
    std::thread::id mainThreadId = std::this_thread::get_id();

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(mainFactory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::move(workerFactory), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    ASSERT_TRUE(mainService->IsInitialized());
    ASSERT_TRUE(workerService->IsInitialized());
    // Main service should run on main thread
    EXPECT_EQ(mainService->GetInitThreadId(), mainThreadId);
    // Worker service should run on a different thread
    EXPECT_NE(workerService->GetInitThreadId(), mainThreadId);
  }

  TEST(LifecycleManager, StartServicesAsync_MultipleNonMainThreadGroups_EachGetsOwnThread)
  {
    auto worker1Service = std::make_shared<MockLifecycleService>();
    auto worker2Service = std::make_shared<MockLifecycleService>();
    auto worker1Factory = std::make_unique<MockLifecycleServiceFactory>(worker1Service);
    auto worker2Factory = std::make_unique<MockLifecycleServiceFactory>(worker2Service);

    ServiceThreadGroupId workerGroup1{1};
    ServiceThreadGroupId workerGroup2{2};

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(worker1Factory), ServiceLaunchPriority(1000), workerGroup1);
    registrations.emplace_back(std::move(worker2Factory), ServiceLaunchPriority(1000), workerGroup2);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    EXPECT_FALSE(worker1Service->IsInitialized());
    EXPECT_FALSE(worker2Service->IsInitialized());

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(worker1Service->IsInitialized());
    EXPECT_TRUE(worker2Service->IsInitialized());
    // Different thread groups should run on different threads
    EXPECT_NE(worker1Service->GetInitThreadId(), worker2Service->GetInitThreadId());
  }

  TEST(LifecycleManager, StartServicesAsync_MixedThreadGroups_PriorityRespected)
  {
    InitializationOrderTracker tracker;

    auto highMain = std::make_shared<MockLifecycleService>("HighMain", &tracker);
    auto highWorker = std::make_shared<MockLifecycleService>("HighWorker", &tracker);
    auto lowMain = std::make_shared<MockLifecycleService>("LowMain", &tracker);
    auto lowWorker = std::make_shared<MockLifecycleService>("LowWorker", &tracker);

    ServiceThreadGroupId workerThreadGroup{1};

    std::vector<ServiceRegistrationRecord> registrations;
    // Scramble the order
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(lowWorker), ServiceLaunchPriority(100), workerThreadGroup);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(highMain), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(lowMain), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<MockLifecycleServiceFactory>(highWorker), ServiceLaunchPriority(1000), workerThreadGroup);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // All high priority services (main + worker) should be initialized before low priority services
    ASSERT_EQ(tracker.Order.size(), 4u);

    // First two should be high priority (HighMain, HighWorker in some order)
    std::vector<std::string> highPriorityNames(tracker.Order.begin(), tracker.Order.begin() + 2);
    std::vector<std::string> lowPriorityNames(tracker.Order.begin() + 2, tracker.Order.end());

    EXPECT_TRUE(std::find(highPriorityNames.begin(), highPriorityNames.end(), "HighMain") != highPriorityNames.end());
    EXPECT_TRUE(std::find(highPriorityNames.begin(), highPriorityNames.end(), "HighWorker") != highPriorityNames.end());
    EXPECT_TRUE(std::find(lowPriorityNames.begin(), lowPriorityNames.end(), "LowMain") != lowPriorityNames.end());
    EXPECT_TRUE(std::find(lowPriorityNames.begin(), lowPriorityNames.end(), "LowWorker") != lowPriorityNames.end());
  }

}
