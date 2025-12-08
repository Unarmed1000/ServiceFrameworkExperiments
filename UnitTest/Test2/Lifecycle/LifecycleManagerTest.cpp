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
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
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
    std::exception_ptr exceptionPtr;

    // Spawn the coroutine - the awaitable methods internally handle their own marshalling
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

    // We need an executor to spawn on - just use a local io_context for the test
    boost::asio::io_context testContext;
    boost::asio::co_spawn(testContext, spawnOp(), boost::asio::detached);

    while (!done)
    {
      // Poll both the test context and the manager
      testContext.poll();
      manager.Poll();
    }

    if (exceptionPtr)
    {
      std::rethrow_exception(exceptionPtr);
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

  // ============================================================================
  // Phase 5: Error Handling & Rollback Tests
  // ============================================================================

  // Failing mock service that throws during initialization
  class FailingMockService : public IServiceControl
  {
  private:
    std::string m_name;
    std::string m_errorMessage;
    InitializationOrderTracker* m_tracker{nullptr};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};

  public:
    FailingMockService(const std::string& name, const std::string& errorMessage, InitializationOrderTracker* tracker = nullptr)
      : m_name(name)
      , m_errorMessage(errorMessage)
      , m_tracker(tracker)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      if (m_tracker)
      {
        m_tracker->RecordInit(m_name);
      }
      throw std::runtime_error(m_errorMessage);
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      m_shutdown = true;
      co_return ServiceShutdownResult::Success;
    }

    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }

    bool IsShutdown() const noexcept
    {
      return m_shutdown.load();
    }
  };

  // Mock factory for failing service
  class FailingMockServiceFactory : public IServiceFactory
  {
  private:
    std::shared_ptr<FailingMockService> m_service;

  public:
    explicit FailingMockServiceFactory(std::shared_ptr<FailingMockService> service)
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

  // Shutdown-tracking mock service
  class ShutdownTrackingMockService : public IServiceControl
  {
  private:
    std::string m_name;
    InitializationOrderTracker* m_initTracker{nullptr};
    InitializationOrderTracker* m_shutdownTracker{nullptr};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};

  public:
    ShutdownTrackingMockService(const std::string& name, InitializationOrderTracker* initTracker, InitializationOrderTracker* shutdownTracker)
      : m_name(name)
      , m_initTracker(initTracker)
      , m_shutdownTracker(shutdownTracker)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      m_initialized = true;
      if (m_initTracker)
      {
        m_initTracker->RecordInit(m_name);
      }
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      m_shutdown = true;
      if (m_shutdownTracker)
      {
        m_shutdownTracker->RecordInit(m_name);    // RecordInit is just RecordOrder
      }
      co_return ServiceShutdownResult::Success;
    }

    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }

    bool IsInitialized() const noexcept
    {
      return m_initialized.load();
    }

    bool IsShutdown() const noexcept
    {
      return m_shutdown.load();
    }
  };

  // Mock factory for shutdown-tracking service
  class ShutdownTrackingMockServiceFactory : public IServiceFactory
  {
  private:
    std::shared_ptr<ShutdownTrackingMockService> m_service;

  public:
    explicit ShutdownTrackingMockServiceFactory(std::shared_ptr<ShutdownTrackingMockService> service)
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

  TEST(LifecycleManager, StartServicesAsync_ServiceInitFails_ThrowsAggregateException)
  {
    auto failingService = std::make_shared<FailingMockService>("FailingService", "Init failed");
    auto factory = std::make_unique<FailingMockServiceFactory>(failingService);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    bool exceptionThrown = false;
    try
    {
      RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });
    }
    catch (const Common::AggregateException& ex)
    {
      exceptionThrown = true;
      EXPECT_GE(ex.GetInnerExceptions().size(), 1u);
    }

    EXPECT_TRUE(exceptionThrown);
  }

  TEST(LifecycleManager, StartServicesAsync_LowerPriorityFails_HigherPriorityRolledBack)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("HighPriority", &initTracker, &shutdownTracker);
    auto lowFailingService = std::make_shared<FailingMockService>("LowPriority", "Low priority init failed", &initTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<FailingMockServiceFactory>(lowFailingService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    bool exceptionThrown = false;
    try
    {
      RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });
    }
    catch (const Common::AggregateException&)
    {
      exceptionThrown = true;
    }

    EXPECT_TRUE(exceptionThrown);
    // High priority service should have been initialized
    EXPECT_TRUE(highService->IsInitialized());
    // High priority service should have been shut down during rollback
    EXPECT_TRUE(highService->IsShutdown());
  }

  TEST(LifecycleManager, StartServicesAsync_MultiplePrioritiesBeforeFailure_AllRolledBack)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("High", &initTracker, &shutdownTracker);
    auto medService = std::make_shared<ShutdownTrackingMockService>("Medium", &initTracker, &shutdownTracker);
    auto lowFailingService = std::make_shared<FailingMockService>("Low", "Low priority init failed", &initTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(medService), ServiceLaunchPriority(500),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<FailingMockServiceFactory>(lowFailingService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    bool exceptionThrown = false;
    try
    {
      RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });
    }
    catch (const Common::AggregateException&)
    {
      exceptionThrown = true;
    }

    EXPECT_TRUE(exceptionThrown);
    // Both high and medium priority services should have been initialized
    EXPECT_TRUE(highService->IsInitialized());
    EXPECT_TRUE(medService->IsInitialized());
    // Both should have been shut down during rollback
    EXPECT_TRUE(highService->IsShutdown());
    EXPECT_TRUE(medService->IsShutdown());
  }

  TEST(LifecycleManager, StartServicesAsync_RollbackOccursInReversePriorityOrder)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("High", &initTracker, &shutdownTracker);
    auto medService = std::make_shared<ShutdownTrackingMockService>("Medium", &initTracker, &shutdownTracker);
    auto lowFailingService = std::make_shared<FailingMockService>("Low", "Low priority init failed", &initTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(medService), ServiceLaunchPriority(500),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<FailingMockServiceFactory>(lowFailingService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    try
    {
      RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });
    }
    catch (const Common::AggregateException&)
    {
      // Expected
    }

    // Init order should be: High -> Medium -> Low (fails)
    ASSERT_EQ(initTracker.Order.size(), 3u);
    EXPECT_EQ(initTracker.Order[0], "High");
    EXPECT_EQ(initTracker.Order[1], "Medium");
    EXPECT_EQ(initTracker.Order[2], "Low");

    // Shutdown (rollback) order should be reverse priority: Medium -> High
    // (Low failed so it's not in the rollback list)
    ASSERT_EQ(shutdownTracker.Order.size(), 2u);
    EXPECT_EQ(shutdownTracker.Order[0], "Medium");
    EXPECT_EQ(shutdownTracker.Order[1], "High");
  }

  TEST(LifecycleManager, StartServicesAsync_HighestPriorityFails_NoRollbackNeeded)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto failingService = std::make_shared<FailingMockService>("High", "High priority init failed", &initTracker);
    auto lowService = std::make_shared<ShutdownTrackingMockService>("Low", &initTracker, &shutdownTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<FailingMockServiceFactory>(failingService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(lowService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    bool exceptionThrown = false;
    try
    {
      RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });
    }
    catch (const Common::AggregateException&)
    {
      exceptionThrown = true;
    }

    EXPECT_TRUE(exceptionThrown);
    // Low priority service should NOT have been initialized (high priority failed first)
    EXPECT_FALSE(lowService->IsInitialized());
    // No rollback should have happened
    EXPECT_EQ(shutdownTracker.Order.size(), 0u);
  }

  // ============================================================================
  // Phase 6: Explicit Shutdown Tests
  // ============================================================================

  TEST(LifecycleManager, ShutdownServicesAsync_AfterSuccessfulStart_ShutsDownAllServices)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("High", &initTracker, &shutdownTracker);
    auto medService = std::make_shared<ShutdownTrackingMockService>("Medium", &initTracker, &shutdownTracker);
    auto lowService = std::make_shared<ShutdownTrackingMockService>("Low", &initTracker, &shutdownTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(medService), ServiceLaunchPriority(500),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(lowService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start all services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Verify all services were initialized
    EXPECT_TRUE(highService->IsInitialized());
    EXPECT_TRUE(medService->IsInitialized());
    EXPECT_TRUE(lowService->IsInitialized());

    // Explicit shutdown
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.ShutdownServicesAsync(); });

    // Verify all services were shut down
    EXPECT_TRUE(highService->IsShutdown());
    EXPECT_TRUE(medService->IsShutdown());
    EXPECT_TRUE(lowService->IsShutdown());
  }

  TEST(LifecycleManager, ShutdownServicesAsync_ShutsDownInReversePriorityOrder)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("High", &initTracker, &shutdownTracker);
    auto medService = std::make_shared<ShutdownTrackingMockService>("Medium", &initTracker, &shutdownTracker);
    auto lowService = std::make_shared<ShutdownTrackingMockService>("Low", &initTracker, &shutdownTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(medService), ServiceLaunchPriority(500),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(lowService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start all services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Verify init order: High -> Medium -> Low
    ASSERT_EQ(initTracker.Order.size(), 3u);
    EXPECT_EQ(initTracker.Order[0], "High");
    EXPECT_EQ(initTracker.Order[1], "Medium");
    EXPECT_EQ(initTracker.Order[2], "Low");

    // Explicit shutdown
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.ShutdownServicesAsync(); });

    // Verify shutdown order: Low -> Medium -> High (reverse of startup)
    ASSERT_EQ(shutdownTracker.Order.size(), 3u);
    EXPECT_EQ(shutdownTracker.Order[0], "Low");
    EXPECT_EQ(shutdownTracker.Order[1], "Medium");
    EXPECT_EQ(shutdownTracker.Order[2], "High");
  }

  TEST(LifecycleManager, ShutdownServicesAsync_CalledTwice_SecondCallIsNoOp)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto service = std::make_shared<ShutdownTrackingMockService>("Service", &initTracker, &shutdownTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(service), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // First shutdown
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.ShutdownServicesAsync(); });

    EXPECT_EQ(shutdownTracker.Order.size(), 1u);

    // Second shutdown should be a no-op (state cleared after first shutdown)
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.ShutdownServicesAsync(); });

    // Should still be 1, not 2 (the service is already unregistered from provider)
    EXPECT_EQ(shutdownTracker.Order.size(), 1u);
  }

  TEST(LifecycleManager, ShutdownServicesAsync_WithNoStartedServices_IsNoOp)
  {
    LifecycleManagerConfig config;
    std::vector<ServiceRegistrationRecord> registrations;

    LifecycleManager manager(config, std::move(registrations));

    // Shutdown without starting - should not throw
    std::vector<std::exception_ptr> errors;
    RunAsyncWithPolling(manager, [&manager, &errors]() -> boost::asio::awaitable<void> { errors = co_await manager.ShutdownServicesAsync(); });

    EXPECT_TRUE(errors.empty());
  }

  TEST(LifecycleManager, ShutdownServicesAsync_ReturnsEmptyVectorOnSuccess)
  {
    auto service = std::make_shared<MockLifecycleService>();
    auto factory = std::make_unique<MockLifecycleServiceFactory>(service);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::move(factory), ServiceLaunchPriority(1000), ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Shutdown and check return value
    std::vector<std::exception_ptr> errors;
    RunAsyncWithPolling(manager, [&manager, &errors]() -> boost::asio::awaitable<void> { errors = co_await manager.ShutdownServicesAsync(); });

    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(service->IsShutdown());
  }

  // Failing shutdown mock service
  class FailingShutdownMockService : public IServiceControl
  {
  private:
    std::string m_name;
    std::string m_errorMessage;
    InitializationOrderTracker* m_initTracker{nullptr};
    InitializationOrderTracker* m_shutdownTracker{nullptr};
    std::atomic<bool> m_initialized{false};

  public:
    FailingShutdownMockService(const std::string& name, const std::string& errorMessage, InitializationOrderTracker* initTracker = nullptr,
                               InitializationOrderTracker* shutdownTracker = nullptr)
      : m_name(name)
      , m_errorMessage(errorMessage)
      , m_initTracker(initTracker)
      , m_shutdownTracker(shutdownTracker)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      m_initialized = true;
      if (m_initTracker)
      {
        m_initTracker->RecordInit(m_name);
      }
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      if (m_shutdownTracker)
      {
        m_shutdownTracker->RecordInit(m_name);
      }
      throw std::runtime_error(m_errorMessage);
    }

    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }

    bool IsInitialized() const noexcept
    {
      return m_initialized.load();
    }
  };

  class FailingShutdownMockServiceFactory : public IServiceFactory
  {
  private:
    std::shared_ptr<FailingShutdownMockService> m_service;

  public:
    explicit FailingShutdownMockServiceFactory(std::shared_ptr<FailingShutdownMockService> service)
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

  TEST(LifecycleManager, ShutdownServicesAsync_ServiceShutdownFails_ReturnsErrors)
  {
    InitializationOrderTracker initTracker;

    auto failingService = std::make_shared<FailingShutdownMockService>("FailingService", "Shutdown failed", &initTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<FailingShutdownMockServiceFactory>(failingService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    EXPECT_TRUE(failingService->IsInitialized());

    // Shutdown - should return errors, not throw
    std::vector<std::exception_ptr> errors;
    RunAsyncWithPolling(manager, [&manager, &errors]() -> boost::asio::awaitable<void> { errors = co_await manager.ShutdownServicesAsync(); });

    EXPECT_FALSE(errors.empty());
  }

  TEST(LifecycleManager, ShutdownServicesAsync_ContinuesAfterServiceShutdownFails)
  {
    InitializationOrderTracker initTracker;
    InitializationOrderTracker shutdownTracker;

    auto highService = std::make_shared<ShutdownTrackingMockService>("High", &initTracker, &shutdownTracker);
    auto failingService = std::make_shared<FailingShutdownMockService>("Failing", "Shutdown failed", &initTracker, &shutdownTracker);
    auto lowService = std::make_shared<ShutdownTrackingMockService>("Low", &initTracker, &shutdownTracker);

    std::vector<ServiceRegistrationRecord> registrations;
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(highService), ServiceLaunchPriority(1000),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<FailingShutdownMockServiceFactory>(failingService), ServiceLaunchPriority(500),
                               ThreadGroupConfig::MainThreadGroupId);
    registrations.emplace_back(std::make_unique<ShutdownTrackingMockServiceFactory>(lowService), ServiceLaunchPriority(100),
                               ThreadGroupConfig::MainThreadGroupId);

    LifecycleManagerConfig config;
    LifecycleManager manager(config, std::move(registrations));

    // Start services
    RunAsyncWithPolling(manager, [&manager]() -> boost::asio::awaitable<void> { co_await manager.StartServicesAsync(); });

    // Shutdown - should continue even after failure
    std::vector<std::exception_ptr> errors;
    RunAsyncWithPolling(manager, [&manager, &errors]() -> boost::asio::awaitable<void> { errors = co_await manager.ShutdownServicesAsync(); });

    // All three should have attempted shutdown (Low, Failing, High in order)
    // Low and High succeed, Failing throws
    ASSERT_EQ(shutdownTracker.Order.size(), 3u);
    EXPECT_EQ(shutdownTracker.Order[0], "Low");
    EXPECT_EQ(shutdownTracker.Order[1], "Failing");
    EXPECT_EQ(shutdownTracker.Order[2], "High");

    // Should have 1 error from the failing service
    EXPECT_EQ(errors.size(), 1u);
  }

}
