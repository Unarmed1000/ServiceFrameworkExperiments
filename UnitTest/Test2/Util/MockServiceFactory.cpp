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

#include <Test2/Util/MockServiceFactory.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <typeindex>

namespace Test2::UnitTest
{
  // ========================================
  // MockService Implementation
  // ========================================

  MockService::MockService(const ServiceCreateInfo& createInfo, MockServiceConfig config)
    : AsyncServiceBase(createInfo)
    , m_config(std::move(config))
  {
    spdlog::debug("MockService::MockService called for {}", m_config.Name);
  }

  boost::asio::awaitable<ServiceInitResult> MockService::InitAsync(const ServiceCreateInfo& /*createInfo*/)
  {
    m_initThreadId = std::this_thread::get_id();
    spdlog::debug("MockService::InitAsync called for {} on thread {}", m_config.Name, m_initThreadId);

    if (m_config.InitTracker)
    {
      m_config.InitTracker->RecordInit(m_config.Name);
    }

    if (m_config.OnInit)
    {
      m_config.OnInit();
    }

    if (m_config.InitShouldFail)
    {
      spdlog::debug("MockService::InitAsync throwing for {}", m_config.Name);
      throw std::runtime_error(m_config.InitErrorMessage);
    }

    m_initialized = true;
    spdlog::debug("MockService::InitAsync returning success for {}", m_config.Name);
    co_return ServiceInitResult::Success;
  }

  boost::asio::awaitable<ServiceShutdownResult> MockService::ShutdownAsync()
  {
    spdlog::debug("MockService::ShutdownAsync called for {}", m_config.Name);

    if (m_config.ShutdownTracker)
    {
      m_config.ShutdownTracker->RecordInit(m_config.Name);    // RecordInit is just RecordOrder
    }

    if (m_config.OnShutdown)
    {
      m_config.OnShutdown();
    }

    if (m_config.ShutdownShouldFail)
    {
      spdlog::debug("MockService::ShutdownAsync throwing for {}", m_config.Name);
      throw std::runtime_error(m_config.ShutdownErrorMessage);
    }

    m_shutdown = true;
    spdlog::debug("MockService::ShutdownAsync returning success for {}", m_config.Name);
    co_return ServiceShutdownResult::Success;
  }

  ProcessResult MockService::Process()
  {
    return ProcessResult::NoSleepLimit();
  }

  // ========================================
  // MockServiceProxy Implementation
  // ========================================

  MockServiceProxy::MockServiceProxy(const ServiceProxyCreateInfo& createInfo, MockServiceConfig config)
    : AsyncServiceProxyBase(createInfo)
    , m_config(std::move(config))
    , m_constructThreadId(std::this_thread::get_id())
  {
    spdlog::debug("MockServiceProxy::MockServiceProxy called for {} on thread {}", m_config.Name, m_constructThreadId);

    // Track proxy construction (not initialization - proxies don't have InitAsync)
    if (m_config.InitTracker)
    {
      m_config.InitTracker->RecordInit(m_config.Name + "_Proxy");
    }

    // Allow custom callback during construction
    if (m_config.OnInit)
    {
      m_config.OnInit();
    }
  }

  ProcessResult MockServiceProxy::Process()
  {
    return ProcessResult::NoSleepLimit();
  }

  // ========================================
  // MockAsyncServiceFactory Implementation
  // ========================================

  MockAsyncServiceFactory::MockAsyncServiceFactory(MockServiceConfig config)
    : AsyncServiceFactory(typeid(ITestInterface))
    , m_config(std::move(config))
  {
  }

  std::shared_ptr<IServiceProxyControl> MockAsyncServiceFactory::CreateProxy(const ServiceProxyCreateInfo& createInfo)
  {
    return std::make_shared<MockServiceProxy>(createInfo, m_config);
  }

  std::shared_ptr<IServiceControl> MockAsyncServiceFactory::Create(const ServiceCreateInfo& createInfo)
  {
    return std::make_shared<MockService>(createInfo, m_config);
  }

}    // namespace Test2::UnitTest
