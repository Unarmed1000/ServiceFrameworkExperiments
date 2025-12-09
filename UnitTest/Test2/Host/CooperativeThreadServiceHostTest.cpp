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

#include <Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <span>
#include <thread>
#include <typeindex>

namespace Test2
{
  using namespace std::chrono_literals;

  // ============================================================================
  // Mock Service for Testing
  // ============================================================================

  class MockCooperativeService : public IServiceControl
  {
  private:
    ProcessResult m_processResult;
    std::atomic<int> m_processCallCount{0};

  public:
    explicit MockCooperativeService(ProcessResult processResult = ProcessResult::NoSleepLimit())
      : m_processResult(processResult)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
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

    void SetProcessResult(ProcessResult result)
    {
      m_processResult = result;
    }
  };

  struct ITestInterface : public IService
  {
  };

  // Mock factory
  class MockCooperativeServiceFactory : public IServiceFactory
  {
  private:
    std::shared_ptr<MockCooperativeService> m_service;

  public:
    explicit MockCooperativeServiceFactory(std::shared_ptr<MockCooperativeService> service)
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
  // Basic Construction and Destruction Tests
  // ============================================================================

  TEST(CooperativeThreadServiceHost, Construction_Succeeds)
  {
    CooperativeThreadServiceHost host;
    // Should construct without throwing
    SUCCEED();
  }

  TEST(CooperativeThreadServiceHost, Destruction_Succeeds)
  {
    auto host = std::make_unique<CooperativeThreadServiceHost>();
    host.reset();
    // Should destruct without throwing
    SUCCEED();
  }

  // ============================================================================
  // Poll Tests
  // ============================================================================

  TEST(CooperativeThreadServiceHost, Poll_WithNoWork_ReturnsZero)
  {
    CooperativeThreadServiceHost host;
    auto count = host.Poll();
    EXPECT_EQ(count, 0u);
  }

  TEST(CooperativeThreadServiceHost, Poll_WithPostedWork_ExecutesHandler)
  {
    CooperativeThreadServiceHost host;
    bool handlerExecuted = false;

    boost::asio::post(host.GetExecutor(), [&handlerExecuted]() { handlerExecuted = true; });

    auto count = host.Poll();
    EXPECT_EQ(count, 1u);
    EXPECT_TRUE(handlerExecuted);
  }

  TEST(CooperativeThreadServiceHost, Poll_WithMultiplePostedWork_ExecutesAllHandlers)
  {
    CooperativeThreadServiceHost host;
    int counter = 0;

    boost::asio::post(host.GetExecutor(), [&counter]() { ++counter; });
    boost::asio::post(host.GetExecutor(), [&counter]() { ++counter; });
    boost::asio::post(host.GetExecutor(), [&counter]() { ++counter; });

    auto count = host.Poll();
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(counter, 3);
  }

  TEST(CooperativeThreadServiceHost, Poll_DoesNotBlock)
  {
    CooperativeThreadServiceHost host;

    auto start = std::chrono::steady_clock::now();
    host.Poll();
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Poll should return almost immediately (< 10ms)
    EXPECT_LT(elapsed, 10ms);
  }

  // ============================================================================
  // Update Tests
  // ============================================================================

  TEST(CooperativeThreadServiceHost, Update_WithNoServices_ReturnsNoSleepLimit)
  {
    CooperativeThreadServiceHost host;
    auto result = host.Update();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  TEST(CooperativeThreadServiceHost, Update_PollsIoContext)
  {
    CooperativeThreadServiceHost host;
    bool handlerExecuted = false;

    boost::asio::post(host.GetExecutor(), [&handlerExecuted]() { handlerExecuted = true; });

    host.Update();
    EXPECT_TRUE(handlerExecuted);
  }

  // ============================================================================
  // Wake Callback Tests
  // ============================================================================

  TEST(CooperativeThreadServiceHost, SetWakeCallback_WithNullptr_Succeeds)
  {
    CooperativeThreadServiceHost host;
    host.SetWakeCallback(nullptr);
    // Should not throw
    SUCCEED();
  }

  TEST(CooperativeThreadServiceHost, SetWakeCallback_WithCallback_Succeeds)
  {
    CooperativeThreadServiceHost host;
    int callCount = 0;
    host.SetWakeCallback([&callCount]() { ++callCount; });
    // Should not throw
    SUCCEED();
  }

  TEST(CooperativeThreadServiceHost, PostWithWake_TriggersWakeCallback)
  {
    CooperativeThreadServiceHost host;
    std::atomic<int> wakeCount{0};

    host.SetWakeCallback([&wakeCount]() { ++wakeCount; });

    host.PostWithWake([]() {});

    EXPECT_EQ(wakeCount.load(), 1);
  }

  TEST(CooperativeThreadServiceHost, PostWithWake_ExecutesHandler)
  {
    CooperativeThreadServiceHost host;
    bool handlerExecuted = false;

    host.SetWakeCallback([]() {});
    host.PostWithWake([&handlerExecuted]() { handlerExecuted = true; });

    host.Poll();
    EXPECT_TRUE(handlerExecuted);
  }

  TEST(CooperativeThreadServiceHost, PostWithWake_FromAnotherThread_TriggersWake)
  {
    CooperativeThreadServiceHost host;
    std::atomic<int> wakeCount{0};

    host.SetWakeCallback([&wakeCount]() { ++wakeCount; });

    std::thread worker([&host]() { host.PostWithWake([]() {}); });

    worker.join();

    EXPECT_EQ(wakeCount.load(), 1);
  }

  TEST(CooperativeThreadServiceHost, PostWithWake_WithNoCallback_DoesNotThrow)
  {
    CooperativeThreadServiceHost host;
    // No wake callback set

    host.PostWithWake([]() {});
    // Should not throw
    SUCCEED();
  }

  // ============================================================================
  // ProcessServices Tests
  // ============================================================================

  TEST(CooperativeThreadServiceHost, ProcessServices_WithNoServices_ReturnsNoSleepLimit)
  {
    CooperativeThreadServiceHost host;
    auto result = host.ProcessServices();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  // ============================================================================
  // Service Registration and Processing Tests (Integration)
  // ============================================================================

  class CooperativeThreadServiceHostServiceTest : public ::testing::Test
  {
  protected:
    CooperativeThreadServiceHost host;
    std::shared_ptr<MockCooperativeService> service1;
    std::shared_ptr<MockCooperativeService> service2;

    void SetUp() override
    {
      service1 = std::make_shared<MockCooperativeService>(ProcessResult::NoSleepLimit());
      service2 = std::make_shared<MockCooperativeService>(ProcessResult::SleepLimit(100ms));
    }

    void RegisterService(std::shared_ptr<MockCooperativeService> service, const std::string& name, uint32_t priority)
    {
      std::vector<StartServiceRecord> services;
      services.emplace_back(name, std::make_unique<MockCooperativeServiceFactory>(service));

      // Run the async registration synchronously using poll
      bool done = false;
      boost::asio::co_spawn(
        host.GetExecutor(),
        [this, services = std::move(services), priority, &done]() mutable -> boost::asio::awaitable<void>
        {
          co_await host.TryStartServicesAsync(std::move(services), ServiceLaunchPriority(priority));
          done = true;
        },
        boost::asio::detached);

      // Poll until registration completes
      while (!done)
      {
        host.Poll();
      }
    }
  };

  TEST_F(CooperativeThreadServiceHostServiceTest, RegisteredService_ProcessIsCalled)
  {
    RegisterService(service1, "TestService", 1000);

    host.ProcessServices();
    EXPECT_EQ(service1->GetProcessCallCount(), 1);
  }

  TEST_F(CooperativeThreadServiceHostServiceTest, Update_CallsServiceProcess)
  {
    RegisterService(service1, "TestService", 1000);

    host.Update();
    EXPECT_EQ(service1->GetProcessCallCount(), 1);
  }

  TEST_F(CooperativeThreadServiceHostServiceTest, ProcessServices_ReturnsServiceResult)
  {
    service1->SetProcessResult(ProcessResult::SleepLimit(50ms));
    RegisterService(service1, "TestService", 1000);

    auto result = host.ProcessServices();
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 50ms);
  }

  TEST_F(CooperativeThreadServiceHostServiceTest, ProcessServices_MergesMultipleServiceResults)
  {
    service1->SetProcessResult(ProcessResult::SleepLimit(100ms));
    service2->SetProcessResult(ProcessResult::SleepLimit(50ms));

    RegisterService(service1, "Service1", 1000);
    RegisterService(service2, "Service2", 500);

    auto result = host.ProcessServices();
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 50ms);    // Should take the shorter duration
  }

  TEST_F(CooperativeThreadServiceHostServiceTest, ProcessServices_QuitOverridesSleepLimit)
  {
    service1->SetProcessResult(ProcessResult::SleepLimit(100ms));
    service2->SetProcessResult(ProcessResult::Quit());

    RegisterService(service1, "Service1", 1000);
    RegisterService(service2, "Service2", 500);

    auto result = host.ProcessServices();
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  TEST_F(CooperativeThreadServiceHostServiceTest, MultipleUpdates_CallsProcessMultipleTimes)
  {
    RegisterService(service1, "TestService", 1000);

    host.Update();
    host.Update();
    host.Update();

    EXPECT_EQ(service1->GetProcessCallCount(), 3);
  }
}
