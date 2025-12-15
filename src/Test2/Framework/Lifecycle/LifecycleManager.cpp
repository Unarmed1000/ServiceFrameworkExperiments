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
#include <Test2/Framework/Exception/WrongThreadException.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManager.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace Test2
{
  LifecycleManager::LifecycleManager(LifecycleManagerConfig config, std::vector<ServiceRegistrationRecord> registrations)
    : m_config(std::move(config))
    , m_registrations(std::move(registrations))
    , m_ownerThreadId(std::this_thread::get_id())
  {
  }

  LifecycleManager::~LifecycleManager()
  {
    m_stopSource.request_stop();
  }

  boost::asio::awaitable<void> LifecycleManager::StartServicesAsync()
  {
    if (m_registrations.empty())
    {
      co_return;
    }

    co_await DoStartServicesAsync(m_registrations, m_startedPriorities, m_mainHost, m_threadHosts, m_stopSource.get_token());
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> LifecycleManager::ShutdownServicesAsync()
  {
    auto allErrors = co_await DoShutdownServicesAsync(std::move(m_startedPriorities), m_mainHost, std::move(m_threadHosts), m_stopSource.get_token());
    m_startedPriorities = {};
    co_return allErrors;
  }

  std::set<ServiceThreadGroupId> LifecycleManager::CollectRequiredThreadGroups(
    const std::map<ServiceLaunchPriority, std::map<ServiceThreadGroupId, std::vector<ServiceRegistrationRecord*>>,
                   std::greater<ServiceLaunchPriority>>& priorityGroups)
  {
    std::set<ServiceThreadGroupId> requiredThreadGroups;
    for (const auto& [priority, threadGroups] : priorityGroups)
    {
      for (const auto& [threadGroupId, regsInGroup] : threadGroups)
      {
        if (threadGroupId != ThreadGroupConfig::MainThreadGroupId)
        {
          requiredThreadGroups.insert(threadGroupId);
        }
      }
    }
    return requiredThreadGroups;
  }

  boost::asio::awaitable<void> LifecycleManager::DoStartServicesAsync(std::vector<ServiceRegistrationRecord>& registrations,
                                                                      std::vector<StartedPriorityRecord>& startedPriorities,
                                                                      CooperativeThreadHost& mainHost, ThreadGroupHostsMap& threadHosts,
                                                                      std::stop_token stopToken)
  {
    // Group registrations by priority, then by thread group
    // Outer map: priority (highest first via std::greater)
    // Inner map: thread group ID -> services for that thread group at this priority
    std::map<ServiceLaunchPriority, std::map<ServiceThreadGroupId, std::vector<ServiceRegistrationRecord*>>, std::greater<ServiceLaunchPriority>>
      priorityGroups;

    for (auto& reg : registrations)
    {
      priorityGroups[reg.Priority][reg.ThreadGroupId].push_back(&reg);
    }

    // First pass: Start all required thread hosts before starting any services
    auto requiredThreadGroups = CollectRequiredThreadGroups(priorityGroups);

    for (const auto& threadGroupId : requiredThreadGroups)
    {
      auto host = std::make_unique<ManagedThreadHost>(mainHost.GetExecutorContext());
      // Start the thread (it will run io_context.run())
      co_await host->StartAsync();
      threadHosts.emplace(threadGroupId, std::move(host));
    }

    // Second pass: Start services in priority order (highest first due to std::greater comparator)
    for (auto& [priority, threadGroups] : priorityGroups)
    {
      // For each thread group at this priority level
      for (auto& [threadGroupId, regsInGroup] : threadGroups)
      {
        std::vector<StartServiceRecord> servicesForGroup;

        for (auto* reg : regsInGroup)
        {
          // Get service name from first supported interface
          auto interfaces = reg->Factory->GetSupportedInterfaces();
          std::string serviceName = interfaces.empty() ? "UnknownService" : interfaces[0].name();

          servicesForGroup.emplace_back(std::move(serviceName), reg->Factory->GetImplFactory());
        }

        if (!servicesForGroup.empty())
        {
          std::exception_ptr startupException;
          try
          {
            std::vector<StartedServiceInfo> startedServices;
            if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
            {
              // Main thread group - use cooperative host
              startedServices = co_await mainHost.GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
            }
            else
            {
              // Non-main thread group - use the pre-started ManagedThreadHost
              auto it = threadHosts.find(threadGroupId);
              if (it == threadHosts.end())
              {
                throw std::runtime_error("Thread host not found for thread group");
              }

              // Start services on the managed thread host
              startedServices = co_await it->second->GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
            }
            [[maybe_unused]] auto& capturedServices = startedServices;    // Aggregate on stack, not used yet

            // Commit the staged services/proxies for this priority
            if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
            {
              co_await mainHost.GetServiceHost()->TryInitializeCompletedAsync();
            }
            else
            {
              auto it = threadHosts.find(threadGroupId);
              if (it != threadHosts.end())
              {
                co_await it->second->GetServiceHost()->TryInitializeCompletedAsync();
              }
            }

            // Track successfully started priority level
            startedPriorities.push_back({priority, threadGroupId});

            // TODO: Phase 6 - Start service proxies after services at this priority
            // This would require extending ServiceRegistrationRecord to include proxy factories
            // Example implementation:
            // std::vector<StartServiceProxyRecord> proxiesForGroup;
            // for (auto* reg : regsInGroup)
            // {
            //   if (reg->ProxyFactory) // If proxy factory is registered
            //   {
            //     auto interfaces = reg->ProxyFactory->GetSupportedInterfaces();
            //     std::string proxyName = interfaces.empty() ? "UnknownProxy" : interfaces[0].name();
            //     proxiesForGroup.emplace_back(std::move(proxyName), reg->ProxyFactory);
            //   }
            // }
            // if (!proxiesForGroup.empty())
            // {
            //   if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
            //   {
            //     co_await mainHost.GetServiceHost()->TryStartServiceProxiesAsync(std::move(proxiesForGroup), priority);
            //   }
            //   else
            //   {
            //     co_await threadHosts[threadGroupId]->GetServiceHost()->TryStartServiceProxiesAsync(std::move(proxiesForGroup), priority);
            //   }
            //   startedProxyPriorities.push_back({priority, threadGroupId});
            // }
          }
          catch (...)
          {
            startupException = std::current_exception();
          }

          // Handle startup failure outside catch block (co_await not allowed in catch)
          if (startupException)
          {
            // Rollback all previously started priority levels
            auto rollbackErrors = co_await DoShutdownServicesAsync(std::move(startedPriorities), mainHost, std::move(threadHosts), stopToken);

            // Combine startup error with any rollback errors
            std::vector<std::exception_ptr> allErrors;
            allErrors.push_back(startupException);
            allErrors.insert(allErrors.end(), rollbackErrors.begin(), rollbackErrors.end());

            throw Common::AggregateException("Service startup failed", std::move(allErrors));
          }
        }
      }
    }

    co_return;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>>
    LifecycleManager::DoShutdownServicesAsync(std::vector<StartedPriorityRecord> startedPriorities, CooperativeThreadHost& mainHost,
                                              ThreadGroupHostsMap threadHosts, std::stop_token stopToken)
  {
    auto mainServiceHost = mainHost.GetServiceHost();
    std::vector<std::exception_ptr> allErrors;

    // Shutdown in reverse order of startup (lowest priority first, then higher)
    AsyncOperationResult serviceShutdownResult;
    try
    {
      serviceShutdownResult = co_await DoShutdownAllServicePrioritiesAsync(std::move(startedPriorities), mainServiceHost, std::move(threadHosts));
      allErrors.insert(allErrors.end(), serviceShutdownResult.Errors.begin(), serviceShutdownResult.Errors.end());
    }
    catch (...)
    {
      auto exception = std::current_exception();
      allErrors.push_back(exception);
      spdlog::error("DoShutdownAllServicePrioritiesAsync threw an exception during shutdown");
      // ThreadHosts were moved, so we have no hosts to shut down
      serviceShutdownResult.ThreadHosts.clear();
    }

    // Shutdown all managed threads in parallel
    try
    {
      auto threadShutdownErrors = co_await DoShutdownThreadHostsAsync(std::move(serviceShutdownResult.ThreadHosts));
      allErrors.insert(allErrors.end(), threadShutdownErrors.begin(), threadShutdownErrors.end());
    }
    catch (...)
    {
      auto exception = std::current_exception();
      allErrors.push_back(exception);
      spdlog::error("DoShutdownThreadHostsAsync threw an exception during shutdown");
    }

    co_return allErrors;
  }

  boost::asio::awaitable<LifecycleManager::AsyncOperationResult>
    LifecycleManager::DoShutdownAllServicePrioritiesAsync(std::vector<StartedPriorityRecord> startedPriorities,
                                                          std::shared_ptr<IServiceHost> mainServiceHost, ThreadGroupHostsMap threadHosts)
  {
    // Group by priority level (use std::less for ascending order, shutting down lowest priority first)
    PriorityMap priorityMap;
    for (const auto& record : startedPriorities)
    {
      priorityMap[record.Priority].push_back(record);
    }

    std::vector<std::exception_ptr> allErrors;
    for (auto& [priority, records] : priorityMap)
    {
      // FIX: threadHosts needs to be shared_ptr or similar to avoid move issues here
      // This is necessary to ensure that if  DoShutdownServicesByPriorityAsync throws we can continue looping with the existing threadHosts
      auto result = co_await DoShutdownServicesByPriorityAsync(std::move(records), mainServiceHost, std::move(threadHosts));
      threadHosts = std::move(result.ThreadHosts);
      allErrors.insert(allErrors.end(), result.Errors.begin(), result.Errors.end());
    }

    co_return AsyncOperationResult{std::move(threadHosts), std::move(allErrors)};
  }

  boost::asio::awaitable<LifecycleManager::AsyncOperationResult>
    LifecycleManager::DoShutdownServicesByPriorityAsync(std::vector<StartedPriorityRecord> records, std::shared_ptr<IServiceHost> mainServiceHost,
                                                        ThreadGroupHostsMap threadHosts)
  {
    std::vector<std::exception_ptr> allErrors;

    // TODO: Phase 7 - Shutdown service proxies BEFORE shutting down services at this priority
    // This would require tracking startedProxyPriorities similar to startedPriorities
    // Example implementation:
    // std::vector<boost::asio::awaitable<std::vector<std::exception_ptr>>> proxyShutdownTasks;
    // for (const auto& record : proxyRecordsAtThisPriority)
    // {
    //   if (record.ThreadGroupId == ThreadGroupConfig::MainThreadGroupId)
    //   {
    //     proxyShutdownTasks.push_back(mainServiceHost->TryShutdownServiceProxiesAsync(record.Priority));
    //   }
    //   else
    //   {
    //     auto hostIt = threadHosts.find(record.ThreadGroupId);
    //     if (hostIt != threadHosts.end())
    //     {
    //       proxyShutdownTasks.push_back(hostIt->second->GetServiceHost()->TryShutdownServiceProxiesAsync(record.Priority));
    //     }
    //   }
    // }
    // auto proxyResults = co_await boost::asio::experimental::make_parallel_group(proxyShutdownTasks).async_wait(...);
    // // Collect proxy shutdown errors...

    // Shutdown all thread groups at this priority level in parallel
    std::vector<boost::asio::awaitable<std::vector<std::exception_ptr>>> shutdownTasks;

    for (const auto& record : records)
    {
      if (record.ThreadGroupId == ThreadGroupConfig::MainThreadGroupId)
      {
        shutdownTasks.push_back(mainServiceHost->TryShutdownServicesAsync(record.Priority));
      }
      else
      {
        auto hostIt = threadHosts.find(record.ThreadGroupId);
        if (hostIt != threadHosts.end())
        {
          shutdownTasks.push_back(hostIt->second->GetServiceHost()->TryShutdownServicesAsync(record.Priority));
        }
      }
    }

    // Wait for all shutdowns at this priority level to complete
    for (auto& task : shutdownTasks)
    {
      try
      {
        auto errors = co_await std::move(task);
        allErrors.insert(allErrors.end(), errors.begin(), errors.end());
      }
      catch (...)
      {
        auto exception = std::current_exception();
        allErrors.push_back(exception);
        spdlog::error("TryShutdownServicesAsync threw an exception during shutdown");
      }
    }

    co_return AsyncOperationResult{std::move(threadHosts), std::move(allErrors)};
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> LifecycleManager::DoShutdownThreadHostsAsync(ThreadGroupHostsMap threadHosts)
  {
    std::vector<std::exception_ptr> allErrors;
    std::vector<boost::asio::awaitable<bool>> threadShutdownTasks;

    // Create shutdown tasks for all thread hosts
    for (auto& [threadGroupId, host] : threadHosts)
    {
      threadShutdownTasks.push_back(host->TryShutdownAsync());
    }

    for (auto& task : threadShutdownTasks)
    {
      try
      {
        co_await std::move(task);
      }
      catch (...)
      {
        allErrors.push_back(std::current_exception());
      }
    }

    co_return allErrors;
  }

  ServiceProvider LifecycleManager::GetServiceProvider()
  {
    // Verify thread access
    const auto currentThreadId = std::this_thread::get_id();
    if (currentThreadId != m_ownerThreadId)
    {
      spdlog::error("LifecycleManager accessed from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
      throw WrongThreadException("LifecycleManager accessed from wrong thread");
    }

    return m_mainHost.GetServiceProvider();
  }

  boost::asio::any_io_executor LifecycleManager::GetExecutor()
  {
    return m_mainHost.GetExecutorContext().GetExecutor();
  }

}
