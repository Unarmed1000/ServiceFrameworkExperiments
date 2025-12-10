#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_LIFECYCLEMANAGER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_LIFECYCLE_LIFECYCLEMANAGER_HPP
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
#include <Test2/Framework/Host/Cooperative/CooperativeThreadHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadHost.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManagerConfig.hpp>
#include <Test2/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceShutdownResult.hpp>
#include <boost/asio/awaitable.hpp>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace Test2
{
  /// @brief Manages the lifecycle of services across multiple thread groups.
  ///
  /// LifecycleManager orchestrates service startup and shutdown across thread groups.
  /// It holds a CooperativeThreadHost for the main thread group (ID 0) and spawns
  /// ManagedThreadHost instances for other thread groups.
  ///
  /// Services are started in priority order (highest first) and shut down in reverse order.
  /// On startup failure, all successfully started services are rolled back in reverse
  /// priority order before throwing an AggregateException with all errors.
  ///
  /// Usage:
  /// 1. Create LifecycleManager with config and service registrations
  /// 2. Call StartServicesAsync() to start all services
  /// 3. Call Update() or Poll() from main loop for cooperative services
  /// 4. Call ShutdownServicesAsync() to cleanly shut down
  class LifecycleManager
  {
    LifecycleManagerConfig m_config;
    CooperativeThreadHost m_mainHost;
    std::vector<ServiceRegistrationRecord> m_registrations;
    std::map<ServiceThreadGroupId, std::unique_ptr<ManagedThreadHost>> m_threadHosts;

    /// @brief Tracks a successfully started priority level for rollback/shutdown.
    struct StartedPriorityRecord
    {
      ServiceLaunchPriority Priority;
      ServiceThreadGroupId ThreadGroupId;
    };

    /// @brief Priority levels that were successfully started, in start order.
    /// Used for rollback on failure and for normal shutdown (processed in reverse).
    std::vector<StartedPriorityRecord> m_startedPriorities;

  public:
    /// @brief Constructs a LifecycleManager with the given configuration and service registrations.
    ///
    /// @param config Configuration options for the lifecycle manager.
    /// @param registrations Service registrations to manage. Ownership is transferred.
    explicit LifecycleManager(LifecycleManagerConfig config, std::vector<ServiceRegistrationRecord> registrations)
      : m_config(std::move(config))
      , m_registrations(std::move(registrations))
    {
    }

    ~LifecycleManager() = default;

    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;
    LifecycleManager(LifecycleManager&&) = delete;
    LifecycleManager& operator=(LifecycleManager&&) = delete;

    /// @brief Starts all registered services in priority order (highest first).
    ///
    /// First ensures all required thread hosts are started, then starts services.
    /// Services are grouped by thread group and started in parallel within each priority level.
    /// On failure, all successfully started services are rolled back in reverse priority order,
    /// and an AggregateException is thrown containing all errors.
    ///
    /// @return Awaitable that completes when all services are started.
    /// @throws AggregateException if any service fails to start (after rollback).
    boost::asio::awaitable<void> StartServicesAsync()
    {
      if (m_registrations.empty())
      {
        co_return;
      }

      // Group registrations by priority, then by thread group
      // Outer map: priority (highest first via std::greater)
      // Inner map: thread group ID -> services for that thread group at this priority
      std::map<ServiceLaunchPriority, std::map<ServiceThreadGroupId, std::vector<ServiceRegistrationRecord*>>, std::greater<ServiceLaunchPriority>>
        priorityGroups;

      for (auto& reg : m_registrations)
      {
        priorityGroups[reg.Priority][reg.ThreadGroupId].push_back(&reg);
      }

      // First pass: Start all required thread hosts before starting any services
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

      for (const auto& threadGroupId : requiredThreadGroups)
      {
        auto host = std::make_unique<ManagedThreadHost>(m_mainHost.GetExecutorContext());
        // Start the thread (it will run io_context.run())
        co_await host->StartAsync();
        m_threadHosts.emplace(threadGroupId, std::move(host));
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

            servicesForGroup.emplace_back(std::move(serviceName), std::move(reg->Factory));
          }

          if (!servicesForGroup.empty())
          {
            std::exception_ptr startupException;
            try
            {
              if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
              {
                // Main thread group - use cooperative host
                co_await m_mainHost.GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
              }
              else
              {
                // Non-main thread group - use the pre-started ManagedThreadHost
                auto it = m_threadHosts.find(threadGroupId);
                if (it == m_threadHosts.end())
                {
                  throw std::runtime_error("Thread host not found for thread group");
                }

                // Start services on the managed thread host
                co_await it->second->GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
              }

              // Track successfully started priority level
              m_startedPriorities.push_back({priority, threadGroupId});
            }
            catch (...)
            {
              startupException = std::current_exception();
            }

            // Handle startup failure outside catch block (co_await not allowed in catch)
            if (startupException)
            {
              // Rollback all previously started priority levels
              auto rollbackErrors = co_await ShutdownServicesAsync();

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

    /// @brief Shuts down all started services in reverse priority order.
    ///
    /// Iterates through started priority levels in reverse order and shuts down
    /// each one by calling TryShutdownServicesAsync on the appropriate host.
    /// After all services are stopped, managed threads are also shut down.
    ///
    /// @return Vector of any exceptions that occurred during shutdown.
    boost::asio::awaitable<std::vector<std::exception_ptr>> ShutdownServicesAsync()
    {
      std::vector<std::exception_ptr> allErrors;

      // Shutdown in reverse order of startup (lowest priority first, then higher)
      for (auto it = m_startedPriorities.rbegin(); it != m_startedPriorities.rend(); ++it)
      {
        std::vector<std::exception_ptr> errors;

        if (it->ThreadGroupId == ThreadGroupConfig::MainThreadGroupId)
        {
          errors = co_await m_mainHost.GetServiceHost()->TryShutdownServicesAsync(it->Priority);
        }
        else
        {
          auto hostIt = m_threadHosts.find(it->ThreadGroupId);
          if (hostIt != m_threadHosts.end())
          {
            errors = co_await hostIt->second->GetServiceHost()->TryShutdownServicesAsync(it->Priority);
          }
        }

        allErrors.insert(allErrors.end(), errors.begin(), errors.end());
      }

      // Clear the tracking
      m_startedPriorities.clear();

      // Shutdown all managed threads
      for (auto& [threadGroupId, host] : m_threadHosts)
      {
        try
        {
          co_await host->TryShutdownAsync();
        }
        catch (...)
        {
          allErrors.push_back(std::current_exception());
        }
      }

      co_return allErrors;
    }

    /// @brief Polls the main thread's io_context and processes all services.
    ///
    /// This is the primary method to call from your main loop. It:
    /// 1. Calls Poll() to process any ready async handlers
    /// 2. Calls ProcessServices() to run service Process() methods
    /// 3. Returns the aggregated ProcessResult with sleep hints
    ///
    /// @return Aggregated ProcessResult from all main thread services.
    ProcessResult Update()
    {
      return m_mainHost.Update();
    }

    /// @brief Process all ready handlers on the main thread without blocking.
    ///
    /// @return The number of handlers that were executed.
    std::size_t Poll()
    {
      return m_mainHost.Poll();
    }

    /// @brief Gets the main thread's cooperative host.
    ///
    /// Use this to access the service host via GetServiceHost().
    ///
    /// @return Reference to the CooperativeThreadHost for the main thread.
    CooperativeThreadHost& GetMainHost()
    {
      return m_mainHost;
    }

    /// @brief Gets the main thread's cooperative host (const version).
    const CooperativeThreadHost& GetMainHost() const
    {
      return m_mainHost;
    }
  };
}

#endif