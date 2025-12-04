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

#include <Test2/Framework/Lifecycle/LifecycleManager.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManagerConfig.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <vector>

namespace Test2
{
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

  public:
    explicit MockLifecycleService(ProcessResult processResult = ProcessResult::NoSleepLimit())
      : m_processResult(processResult)
    {
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      m_initialized = true;
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

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      static const std::type_info* interfaces[] = {&typeid(ITestInterface)};
      return std::span<const std::type_info>(reinterpret_cast<const std::type_info*>(interfaces), 1);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
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

}
