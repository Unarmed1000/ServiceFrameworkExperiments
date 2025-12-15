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

#include "ServiceHostBase.hpp"
#include <Common/AggregateException.hpp>
#include <Test2/Framework/Exception/InvalidServiceFactoryException.hpp>
#include <Test2/Framework/Exception/WrongThreadException.hpp>
#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Host/StartServiceProxyRecord.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Host/StartedServiceInfo.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Provider/ServiceProviderProxy.hpp>
#include <Test2/Framework/Provider/ServiceProviderServiceInstance.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceProxyControl.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Framework/Service/ServiceProxyCreateInfo.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <cassert>
#include "Managed/ManagedThreadServiceProvider.hpp"

namespace Test2
{
  ServiceHostBase::ServiceHostBase()
    : m_ownerThreadId(std::this_thread::get_id())
    , m_provider(std::make_shared<ManagedThreadServiceProvider>())
  {
    spdlog::trace("ServiceHostBase Created at {}", m_ownerThreadId);
  }

  ServiceHostBase::~ServiceHostBase()
  {
    // Assert that destructor is called from the owner thread (debug builds)
    // Also log error in release builds for diagnostics
    if (std::this_thread::get_id() != m_ownerThreadId)
    {
      spdlog::error("ServiceHostBase destroyed from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, std::this_thread::get_id());
    }
    assert(std::this_thread::get_id() == m_ownerThreadId && "ServiceHostBase must be destroyed on its owner thread");

    // Verify shutdown assumptions - log warnings for any violations
    {
      const auto serviceCount = m_provider->GetServiceCount();
      if (serviceCount > 0)
      {
        spdlog::warn("ServiceHostBase destroyed with {} services still registered", serviceCount);
      }
    }
    m_ioContext.stop();
  }

  std::thread::id ServiceHostBase::GetOwnerThreadId() const noexcept
  {
    return m_ownerThreadId;
  }

  std::shared_ptr<IServiceProvider> ServiceHostBase::GetServiceProvider() const
  {
    ValidateThreadAccess();
    return m_provider;
  }

  void ServiceHostBase::RequestShutdown()
  {
    ValidateThreadAccess();
    m_shutdownRequested = true;
  }

  boost::asio::awaitable<std::vector<StartedServiceInfo>> ServiceHostBase::TryStartServicesAsync(std::vector<StartServiceRecord> services,
                                                                                                 ServiceLaunchPriority currentPriority)
  {
    ValidateThreadAccess();

    // Handle empty service list
    if (services.empty())
    {
      spdlog::warn("TryStartServicesAsync called with empty service list at priority {}", currentPriority.GetValue());
      co_return std::vector<StartedServiceInfo>{};
    }

    // Validate service factories
    ValidateServiceFactories(services);

    // Create proxy for provider - can be cleared on failure
    auto providerProxy = std::make_shared<ServiceProviderProxy>(m_provider);
    std::weak_ptr<IServiceProvider> providerWeak = providerProxy;
    ServiceProvider serviceProvider(providerWeak);
    ServiceCreateInfo createInfo(serviceProvider);

    std::vector<ServiceInitRecord> initRecords;

    try
    {
      // Phase 1: Create all service instances
      CreateServiceInstances(services, createInfo, initRecords);

      // Phase 2: Initialize all services
      co_await InitializeServices(initRecords, createInfo);

      // Phase 3: Handle failures with rollback or register successful services
      co_await ProcessInitializationResults(initRecords, currentPriority, providerProxy);

      // Build return value with executor contexts for successfully started services
      std::vector<StartedServiceInfo> startedServices;
      startedServices.reserve(initRecords.size());

      auto executor = GetExecutor();
      for (auto& record : initRecords)
      {
        if (record.InitSucceeded)
        {
          // Cast to IService for the executor context
          std::shared_ptr<IService> servicePtr = std::static_pointer_cast<IService>(record.Service);
          ExecutorContext<IService> executorContext(servicePtr, executor);
          startedServices.emplace_back(std::move(executorContext));
        }
      }

      co_return startedServices;
    }
    catch (...)
    {
      // Clear the proxy on any exception
      providerProxy->Clear();
      throw;
    }
  }

  boost::asio::awaitable<void> ServiceHostBase::TryStartServiceProxiesAsync(std::vector<StartServiceProxyRecord> services,
                                                                            ServiceLaunchPriority currentPriority)
  {
    ValidateThreadAccess();

    // Handle empty service list
    if (services.empty())
    {
      spdlog::warn("TryStartServiceProxiesAsync called with empty service list at priority {}", currentPriority.GetValue());
      co_return;
    }

    // Validate proxy factories
    ValidateServiceProxyFactories(services);

    // Create provider proxy for dependency access
    auto providerProxy = std::make_shared<ServiceProviderProxy>(m_provider);
    std::weak_ptr<IServiceProvider> providerWeak = providerProxy;
    ServiceProvider serviceProvider(providerWeak);

    // Create a simple DispatchContext - proxies are created on the same thread as the host
    // Use empty/null contexts since proxies don't need cross-thread dispatch at creation time
    ExecutorContext<IService> emptyTarget(nullptr, GetExecutor());
    ExecutorContext<ILifeTracker> emptySource(nullptr, GetExecutor());
    DispatchContext<ILifeTracker, IService> dispatchContext(emptySource, emptyTarget);

    ServiceProxyCreateInfo createInfo(dispatchContext, serviceProvider);

    std::vector<std::shared_ptr<IServiceProxyControl>> createdProxies;
    std::vector<ServiceProviderServiceInstance> proxyInfos;
    std::vector<std::exception_ptr> creationErrors;

    try
    {
      // Create all proxy instances
      for (auto& proxyRecord : services)
      {
        try
        {
          spdlog::info("Creating proxy: {}", proxyRecord.ServiceName);

          // Get the supported interfaces
          auto interfaces = proxyRecord.Factory->GetSupportedInterfaces();
          if (interfaces.empty())
          {
            throw std::runtime_error(fmt::format("Proxy factory for '{}' has no supported interfaces", proxyRecord.ServiceName));
          }

          // Create proxy for the first supported interface
          auto proxy = proxyRecord.Factory->CreateProxy(createInfo);
          if (!proxy)
          {
            throw std::runtime_error(fmt::format("Factory returned null proxy for '{}'", proxyRecord.ServiceName));
          }

          // Cast to IServiceProxyControl for provider registration
          auto proxyControl = std::dynamic_pointer_cast<IServiceProxyControl>(proxy);
          if (!proxyControl)
          {
            throw std::runtime_error(fmt::format("Proxy '{}' does not implement IServiceProxyControl", proxyRecord.ServiceName));
          }

          // Build ServiceProviderServiceInstance for provider registration
          ServiceProviderServiceInstance proxyInfo;
          proxyInfo.Type = InstanceType::Proxy;
          proxyInfo.Instance = proxyControl;
          proxyInfo.SupportedInterfaces = std::vector<std::type_index>(interfaces.begin(), interfaces.end());

          createdProxies.push_back(proxy);
          proxyInfos.push_back(std::move(proxyInfo));
          spdlog::info("Proxy created successfully: {}", proxyRecord.ServiceName);
        }
        catch (const std::exception& ex)
        {
          creationErrors.push_back(std::current_exception());
          spdlog::error("Proxy creation failed: {} - {}", proxyRecord.ServiceName, ex.what());
        }
        catch (...)
        {
          creationErrors.push_back(std::current_exception());
          spdlog::error("Proxy creation failed: {} - unknown exception", proxyRecord.ServiceName);
        }
      }

      // If any creation failed, throw aggregate exception
      if (!creationErrors.empty())
      {
        throw Common::AggregateException("One or more proxy creations failed", std::move(creationErrors));
      }

      // Register proxies with provider so services can access them
      if (!proxyInfos.empty())
      {
        const auto proxyCount = proxyInfos.size();
        m_provider->RegisterPriorityGroup(InstanceType::Proxy, currentPriority, std::move(proxyInfos));
        // Note: Not committing here - caller will invoke TryInitializeCompletedAsync to commit
        spdlog::info("Created and registered {} proxies at priority {}", proxyCount, currentPriority.GetValue());
      }

      co_return;
    }
    catch (...)
    {
      // Clear the provider proxy on any exception
      providerProxy->Clear();
      throw;
    }
  }

  boost::asio::awaitable<void> ServiceHostBase::TryInitializeCompletedAsync()
  {
    ValidateThreadAccess();

    // Commit any staged services/proxies to make them available
    // On failure, propagate exception - caller must use shutdown methods to cleanup
    m_provider->CommitStagedPriority();

    co_return;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> ServiceHostBase::TryShutdownServiceProxiesAsync(ServiceLaunchPriority priority)
  {
    ValidateThreadAccess();

    std::vector<std::exception_ptr> shutdownFailures;

    // First, discard any staged proxies at this priority
    // Check if there are staged instances and if they match this priority
    if (m_provider->GetStagedServiceCount() > 0)
    {
      // Discard only PROXY instances - services handled separately
      m_provider->DiscardStagedPriority(InstanceType::Proxy);
      spdlog::info("Discarded staged proxy instances during proxy shutdown at priority {}", priority.GetValue());
    }

    // Then unregister committed proxies at this priority level
    auto proxies = m_provider->UnregisterPriorityGroup(InstanceType::Proxy, priority);

    if (proxies.empty())
    {
      co_return shutdownFailures;
    }

    spdlog::info("Shutting down {} committed proxies at priority {}", proxies.size(), priority.GetValue());

    // No actual shutdown logic needed since proxies don't have ShutdownAsync
    // They're just cleaned up when references are released
    co_return shutdownFailures;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> ServiceHostBase::TryShutdownServicesAsync(ServiceLaunchPriority priority)
  {
    ValidateThreadAccess();

    std::vector<std::exception_ptr> shutdownFailures;

    // VALIDATION: There should NOT be any staged proxies at this point
    // Proxies should have been handled by TryShutdownServiceProxiesAsync
    if (m_provider->GetStagedInstanceCount(InstanceType::Proxy) > 0)
    {
      spdlog::error(
        "CRITICAL: Staged proxies detected during service shutdown at priority {} - proxies should have been handled by "
        "TryShutdownServiceProxiesAsync",
        priority.GetValue());
    }

    // First, discard any staged services at this priority
    // Note: Proxies handled separately in TryShutdownServiceProxiesAsync
    // This only discards SERVICE instances
    if (m_provider->GetStagedServiceCount() > 0)
    {
      m_provider->DiscardStagedPriority(InstanceType::Service);
      spdlog::info("Discarded staged service instances during service shutdown at priority {}", priority.GetValue());
    }

    // Then unregister committed services at this priority level
    auto services = m_provider->UnregisterPriorityGroup(InstanceType::Service, priority);

    if (services.empty())
    {
      co_return shutdownFailures;
    }

    spdlog::info("Shutting down {} committed services at priority {} (reverse order)", services.size(), priority.GetValue());

    // Shutdown in reverse order - services have ShutdownAsync
    for (auto it = services.rbegin(); it != services.rend(); ++it)
    {
      // Cast to IServiceControl (we know these are services)
      auto serviceControl = std::dynamic_pointer_cast<IServiceControl>(it->Instance);
      if (!serviceControl)
      {
        spdlog::error("Failed to cast service instance to IServiceControl during shutdown");
        shutdownFailures.push_back(std::make_exception_ptr(std::runtime_error("Service instance does not implement IServiceControl")));
        continue;
      }

      try
      {
        auto result = co_await serviceControl->ShutdownAsync();
        if (result != ServiceShutdownResult::Success)
        {
          spdlog::warn("Service shutdown returned non-success result: {}", static_cast<int>(result));
        }
      }
      catch (...)
      {
        shutdownFailures.push_back(std::current_exception());
        spdlog::error("Exception during service shutdown");
      }
    }

    co_return shutdownFailures;
  }

  void ServiceHostBase::ValidateThreadAccess() const
  {
    const auto currentThreadId = std::this_thread::get_id();
    if (currentThreadId != m_ownerThreadId)
    {
      spdlog::error("ServiceHostBase accessed from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
      throw WrongThreadException("ServiceHostBase accessed from wrong thread");
    }
  }

  ProcessResult ServiceHostBase::DoProcessServices()
  {
    ValidateThreadAccess();
    return m_provider->Process();
  }

  std::size_t ServiceHostBase::DoPoll()
  {
    ValidateThreadAccess();
    return m_ioContext.poll();
  }

  ProcessResult ServiceHostBase::DoUpdate()
  {
    ValidateThreadAccess();
    DoPoll();
    return DoProcessServices();
  }

  void ServiceHostBase::DoRun()
  {
    ValidateThreadAccess();
    spdlog::trace("ServiceHostBase starting io_context run loop at {}", static_cast<void*>(this));
    m_ioContext.run();
    spdlog::trace("ServiceHostBase io_context run loop has exited at {}", static_cast<void*>(this));
  }

  void ServiceHostBase::ValidateServiceFactories(const std::vector<StartServiceRecord>& services)
  {
    ValidateThreadAccess();

    for (const auto& serviceRecord : services)
    {
      if (!serviceRecord.Factory)
      {
        throw InvalidServiceFactoryException(fmt::format("Invalid service factory in StartServiceRecord for service: {}", serviceRecord.ServiceName));
      }
    }
  }

  void ServiceHostBase::ValidateServiceProxyFactories(const std::vector<StartServiceProxyRecord>& services)
  {
    ValidateThreadAccess();

    for (const auto& serviceRecord : services)
    {
      if (!serviceRecord.Factory)
      {
        throw InvalidServiceFactoryException(
          fmt::format("Invalid service proxy factory in StartServiceProxyRecord for service: {}", serviceRecord.ServiceName));
      }
    }
  }

  void ServiceHostBase::CreateServiceInstances(std::vector<StartServiceRecord>& services, const ServiceCreateInfo& createInfo,
                                               std::vector<ServiceInitRecord>& initRecords)
  {
    ValidateThreadAccess();

    initRecords.reserve(services.size());

    for (auto& serviceRecord : services)
    {
      ServiceInitRecord record;
      record.ServiceName = serviceRecord.ServiceName;

      spdlog::info("Creating service: {}", serviceRecord.ServiceName);

      // Get supported interfaces from factory
      auto supportedInterfaces = serviceRecord.Factory->GetSupportedInterfaces();
      if (supportedInterfaces.empty())
      {
        throw std::invalid_argument(fmt::format("Factory for service '{}' reports no supported interfaces", serviceRecord.ServiceName));
      }

      // Create service instance
      record.Service = serviceRecord.Factory->Create(createInfo);
      if (!record.Service)
      {
        throw std::runtime_error(fmt::format("Factory for service '{}' returned null service", serviceRecord.ServiceName));
      }

      // Prepare InstanceInfo
      record.InstanceInfo.Service = record.Service;
      record.InstanceInfo.SupportedInterfaces.reserve(supportedInterfaces.size());
      for (const auto& typeIndex : supportedInterfaces)
      {
        record.InstanceInfo.SupportedInterfaces.push_back(typeIndex);
      }

      initRecords.push_back(std::move(record));
    }
  }

  boost::asio::awaitable<void> ServiceHostBase::InitializeServices(std::vector<ServiceInitRecord>& initRecords, const ServiceCreateInfo& createInfo)
  {
    ValidateThreadAccess();

    for (auto& record : initRecords)
    {
      try
      {
        spdlog::info("Initializing service: {}", record.ServiceName);

        auto initResult = co_await record.Service->InitAsync(createInfo);
        if (initResult != ServiceInitResult::Success)
        {
          throw std::runtime_error("Service '" + record.ServiceName +
                                   "' initialization failed with result: " + std::to_string(static_cast<int>(initResult)));
        }

        record.InitSucceeded = true;
        spdlog::info("Service initialized successfully: {}", record.ServiceName);
      }
      catch (...)
      {
        record.InitException = std::current_exception();
        spdlog::error("Service initialization failed: {}", record.ServiceName);
      }
    }

    co_return;
  }

  boost::asio::awaitable<void> ServiceHostBase::ProcessInitializationResults(std::vector<ServiceInitRecord>& initRecords,
                                                                             ServiceLaunchPriority currentPriority,
                                                                             std::shared_ptr<ServiceProviderProxy> providerProxy)
  {
    ValidateThreadAccess();

    // Collect failures and successful services
    std::vector<std::exception_ptr> initFailures;
    std::vector<std::shared_ptr<IServiceControl>> successfulServices;

    for (const auto& record : initRecords)
    {
      if (record.InitException)
      {
        initFailures.push_back(record.InitException);
      }
      else if (record.InitSucceeded)
      {
        successfulServices.push_back(record.Service);
      }
    }

    // If any initializations failed, perform rollback
    if (!initFailures.empty())
    {
      auto shutdownFailures = co_await RollbackServices(successfulServices);

      // Clear the proxy to prevent further service access
      providerProxy->Clear();

      // Combine initialization and shutdown failures
      std::vector<std::exception_ptr> allFailures = std::move(initFailures);
      allFailures.insert(allFailures.end(), shutdownFailures.begin(), shutdownFailures.end());

      // Throw aggregate exception with all failures
      throw Common::AggregateException("Service initialization failed", std::move(allFailures));
    }

    // All services initialized successfully - register with provider
    RegisterServicesWithProvider(initRecords, currentPriority);
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>>
    ServiceHostBase::RollbackServices(const std::vector<std::shared_ptr<IServiceControl>>& successfulServices)
  {
    ValidateThreadAccess();
    spdlog::warn("Performing rollback of {} successful services", successfulServices.size());

    std::vector<std::exception_ptr> shutdownFailures;

    // Shutdown in reverse order
    for (auto it = successfulServices.rbegin(); it != successfulServices.rend(); ++it)
    {
      try
      {
        auto shutdownResult = co_await (*it)->ShutdownAsync();
        if (shutdownResult != ServiceShutdownResult::Success)
        {
          spdlog::warn("Service shutdown during rollback returned non-success result: {}", static_cast<int>(shutdownResult));
        }
      }
      catch (...)
      {
        shutdownFailures.push_back(std::current_exception());
        spdlog::error("Exception during service shutdown in rollback");
      }
    }

    co_return shutdownFailures;
  }

  void ServiceHostBase::RegisterServicesWithProvider(std::vector<ServiceInitRecord>& initRecords, ServiceLaunchPriority currentPriority)
  {
    ValidateThreadAccess();

    std::vector<ServiceProviderServiceInstance> serviceInfos;
    serviceInfos.reserve(initRecords.size());

    for (auto& record : initRecords)
    {
      // Convert ServiceInstanceInfo to ServiceProviderServiceInstance
      ServiceProviderServiceInstance providerInfo;
      providerInfo.Type = InstanceType::Service;
      providerInfo.Instance = record.InstanceInfo.Service;
      providerInfo.SupportedInterfaces = std::move(record.InstanceInfo.SupportedInterfaces);
      serviceInfos.push_back(std::move(providerInfo));
    }

    m_provider->RegisterPriorityGroup(InstanceType::Service, currentPriority, std::move(serviceInfos));
    // Note: Not committing here - caller will invoke TryInitializeCompletedAsync to commit
    // This allows services and proxies at the same priority to be staged together

    spdlog::info("Successfully initialized and registered {} services at priority {}", initRecords.size(), currentPriority.GetValue());
  }
}
