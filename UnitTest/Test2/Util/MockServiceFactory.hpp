#ifndef SERVICE_FRAMEWORK_TEST2_UNITTEST_UTIL_MOCKSERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_UNITTEST_UTIL_MOCKSERVICEFACTORY_HPP
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

#include <Test2/Framework/Service/Async/AsyncServiceBase.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceProxyBase.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceProxyFactory.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceProxyControl.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Framework/Service/ServiceProxyCreateInfo.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Test2::UnitTest
{
  /// @brief Tracks initialization order for testing service lifecycle sequencing
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

  /// @brief Test interface marker for mock services
  struct ITestInterface : public IService
  {
  };

  /// @brief Configuration for controlling mock service behavior
  struct MockServiceConfig
  {
    std::string Name;
    bool InitShouldFail = false;
    bool ShutdownShouldFail = false;
    std::string InitErrorMessage = "Init failed";
    std::string ShutdownErrorMessage = "Shutdown failed";
    InitializationOrderTracker* InitTracker = nullptr;
    InitializationOrderTracker* ShutdownTracker = nullptr;
    std::function<void()> OnInit;
    std::function<void()> OnShutdown;

    MockServiceConfig(std::string name = "MockService")
      : Name(std::move(name))
    {
    }
  };

  /// @brief Mock service implementation for testing
  class MockService
    : public AsyncServiceBase
    , public ITestInterface
  {
  private:
    MockServiceConfig m_config;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown{false};
    std::thread::id m_initThreadId;

  public:
    explicit MockService(const ServiceCreateInfo& createInfo, MockServiceConfig config = {});

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& createInfo) override;
    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override;
    ProcessResult Process() override;

    [[nodiscard]] bool IsInitialized() const noexcept
    {
      return m_initialized.load();
    }
    [[nodiscard]] bool IsShutdown() const noexcept
    {
      return m_shutdown.load();
    }
    [[nodiscard]] std::thread::id GetInitThreadId() const noexcept
    {
      return m_initThreadId;
    }
    [[nodiscard]] const std::string& GetName() const noexcept
    {
      return m_config.Name;
    }
  };

  /// @brief Mock service proxy implementation for testing
  /// Proxies do NOT have InitAsync/ShutdownAsync lifecycle methods - they are created and used immediately
  class MockServiceProxy
    : public AsyncServiceProxyBase
    , public ITestInterface
  {
  private:
    MockServiceConfig m_config;
    std::thread::id m_constructThreadId;

  public:
    explicit MockServiceProxy(const ServiceProxyCreateInfo& createInfo, MockServiceConfig config = {});

    ProcessResult Process() override;

    [[nodiscard]] std::thread::id GetConstructThreadId() const noexcept
    {
      return m_constructThreadId;
    }
    [[nodiscard]] const std::string& GetName() const noexcept
    {
      return m_config.Name;
    }
  };

  /// @brief Reusable mock service implementation factory
  class MockServiceImplFactory : public AsyncServiceImplFactory
  {
  private:
    MockServiceConfig m_config;

  public:
    explicit MockServiceImplFactory(MockServiceConfig config);

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& createInfo) override;
  };

  /// @brief Reusable mock service proxy factory
  class MockServiceProxyFactory : public AsyncServiceProxyFactory
  {
  private:
    MockServiceConfig m_config;

  public:
    explicit MockServiceProxyFactory(MockServiceConfig config);

    std::shared_ptr<IServiceProxyControl> CreateProxy(const std::type_index& type, const ServiceProxyCreateInfo& createInfo) override;
  };

}    // namespace Test2::UnitTest

#endif
