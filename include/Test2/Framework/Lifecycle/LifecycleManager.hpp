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
#include <stop_token>
#include <vector>

namespace Test2
{
  // FIX: since we own the boost asio executor for this thread we should probably just make the ShutdownServicesAsync a synchronous method
  //      That internally drives the asio context to completion but from the outside it would be a blocking call.
  //      This ensures that the io_context is available until the shutdown has finished!
  //      Currently the shutdown sequence will not work properly if the user calls ShutdownServicesAsync and then immediately destroys the
  //      LifecycleManager since the io_context will be destroyed before the shutdown coroutines complete.

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
    /// @brief Tracks a successfully started priority level for rollback/shutdown.
    struct StartedPriorityRecord
    {
      ServiceLaunchPriority Priority;
      ServiceThreadGroupId ThreadGroupId;
    };

    using PriorityMap = std::map<ServiceLaunchPriority, std::vector<StartedPriorityRecord>, std::less<ServiceLaunchPriority>>;
    using ThreadGroupHostsMap = std::map<ServiceThreadGroupId, std::unique_ptr<ManagedThreadHost>>;

    struct AsyncOperationResult
    {
      ThreadGroupHostsMap ThreadHosts;
      std::vector<std::exception_ptr> Errors;
    };

    LifecycleManagerConfig m_config;
    CooperativeThreadHost m_mainHost;
    std::vector<ServiceRegistrationRecord> m_registrations;
    ThreadGroupHostsMap m_threadHosts;


    /// @brief Priority levels that were successfully started, in start order.
    /// Used for rollback on failure and for normal shutdown (processed in reverse).
    std::vector<StartedPriorityRecord> m_startedPriorities;

    /// @brief Stop source to signal when the LifecycleManager is being destroyed.
    std::stop_source m_stopSource;

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

    ~LifecycleManager()
    {
      m_stopSource.request_stop();
    }

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

      co_await DoStartServicesAsync(m_registrations, m_startedPriorities, m_mainHost, m_threadHosts, m_stopSource.get_token());
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
      auto allErrors =
        co_await DoShutdownServicesAsync(std::move(m_startedPriorities), m_mainHost, std::move(m_threadHosts), m_stopSource.get_token());
      m_startedPriorities = {};
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

  private:
    /// @brief Collects all unique non-main thread group IDs from the priority groups.
    ///
    /// @param priorityGroups Map of priorities to thread groups with their service registrations.
    /// @return Set of thread group IDs that require managed thread hosts.
    static std::set<ServiceThreadGroupId>
      CollectRequiredThreadGroups(const std::map<ServiceLaunchPriority, std::map<ServiceThreadGroupId, std::vector<ServiceRegistrationRecord*>>,
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

    /// @brief Performs the actual startup of services across thread groups.
    ///
    /// @param registrations Vector of service registrations to start.
    /// @param startedPriorities Output vector to track successfully started priority levels.
    /// @param mainHost Reference to the main cooperative thread host.
    /// @param threadHosts Map of managed thread hosts (will be populated as needed).
    /// @param stopToken Stop token to indicate if the LifecycleManager object has died.
    /// @throws AggregateException if any service fails to start (after rollback).
    static boost::asio::awaitable<void> DoStartServicesAsync(std::vector<ServiceRegistrationRecord>& registrations,
                                                             std::vector<StartedPriorityRecord>& startedPriorities, CooperativeThreadHost& mainHost,
                                                             ThreadGroupHostsMap& threadHosts, std::stop_token stopToken)
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
                co_await mainHost.GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
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
                co_await it->second->GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
              }

              // Track successfully started priority level
              startedPriorities.push_back({priority, threadGroupId});
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

    /// @brief Performs the actual shutdown of services and managed threads.
    ///
    /// Handles exceptions from both service shutdown and thread shutdown operations.
    /// All exceptions are caught, logged, and added to the returned error vector.
    ///
    /// @param startedPriorities Vector of started priority records to shut down in reverse order.
    /// @param mainHost Reference to the main cooperative thread host.
    /// @param threadHosts Map of managed thread hosts (ownership transferred).
    /// @param stopToken Stop token to indicate if the LifecycleManager object has died.
    /// @return Vector of any exceptions that occurred during shutdown.
    static boost::asio::awaitable<std::vector<std::exception_ptr>> DoShutdownServicesAsync(std::vector<StartedPriorityRecord> startedPriorities,
                                                                                           CooperativeThreadHost& mainHost,
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
        serviceShutdownResult.ThreadHosts = {};
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

    /// @brief Shuts down services for all priority levels in reverse order of startup.
    ///
    /// Processes priority levels sequentially, transferring threadHosts ownership between
    /// priority levels to ensure proper resource management.
    ///
    /// @param startedPriorities Vector of started priority records to shut down in reverse order.
    /// @param mainServiceHost Reference to the main thread service host.
    /// @param threadHosts Map of managed thread hosts (ownership transferred).
    /// @return AsyncOperationResult containing threadHosts and any exceptions that occurred.
    static boost::asio::awaitable<AsyncOperationResult> DoShutdownAllServicePrioritiesAsync(std::vector<StartedPriorityRecord> startedPriorities,
                                                                                            std::shared_ptr<IThreadSafeServiceHost> mainServiceHost,
                                                                                            ThreadGroupHostsMap threadHosts)
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

    /// @brief Shuts down services for a specific priority level across all thread groups in parallel.
    ///
    /// Creates shutdown tasks for all thread groups at this priority level and awaits them.
    /// Each task is individually wrapped in exception handling to ensure all errors are captured.
    ///
    /// @param records Vector of priority records for the same priority level.
    /// @param mainServiceHost Reference to the main thread service host.
    /// @param threadHosts Map of managed thread hosts (ownership transferred and returned).
    /// @return AsyncOperationResult containing the threadHosts (for chaining) and any exceptions that occurred during shutdown.
    /// @note This does not need a stop token since it owns the lifetime of everything it touches at this point in time.
    static boost::asio::awaitable<AsyncOperationResult> DoShutdownServicesByPriorityAsync(std::vector<StartedPriorityRecord> records,
                                                                                          std::shared_ptr<IThreadSafeServiceHost> mainServiceHost,
                                                                                          ThreadGroupHostsMap threadHosts)
    {
      std::vector<std::exception_ptr> allErrors;

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

    /// @brief Shuts down all managed thread hosts in parallel.
    ///
    /// Creates shutdown tasks for all thread hosts and awaits them sequentially.
    /// Each shutdown operation is wrapped in exception handling to capture all errors.
    ///
    /// @param threadHosts Map of managed thread hosts to shut down (ownership transferred).
    /// @return Vector of any exceptions that occurred during thread shutdown.
    /// @note This does not need a stop token since it owns the lifetime of everything it touches at this point in time.
    static boost::asio::awaitable<std::vector<std::exception_ptr>> DoShutdownThreadHostsAsync(ThreadGroupHostsMap threadHosts)
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
  };
}

#endif