#ifndef SERVICE_FRAMEWORK_TEST4_FRAMEWORK_LIFECYCLE_LIFECYCLEMANAGER_HPP
#define SERVICE_FRAMEWORK_TEST4_FRAMEWORK_LIFECYCLE_LIFECYCLEMANAGER_HPP
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
#include <Test4/Framework/Config/ThreadGroupConfig.hpp>
#include <Test4/Framework/Host/Cooperative/CooperativeThreadHost.hpp>
#include <Test4/Framework/Host/Managed/ManagedThreadHost.hpp>
#include <Test4/Framework/Host/StartServiceRecord.hpp>
#include <Test4/Framework/Lifecycle/LifecycleManagerConfig.hpp>
#include <Test4/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test4/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test4/Framework/Service/ProcessResult.hpp>
#include <Test4/Framework/Service/ServiceShutdownResult.hpp>
#include <boost/thread/future.hpp>
#include <map>
#include <memory>
#include <stop_token>
#include <vector>

namespace Test4
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
  /// The stop_source is used to signal cancellation to all async operations if the
  /// LifecycleManager is being destroyed or if operations should be cancelled.
  ///
  /// Usage:
  /// 1. Create LifecycleManager with config and service registrations (stack or heap)
  /// 2. Call StartServicesAsync() to start all services (returns boost::future<void>)
  /// 3. Call Update() or Poll() from main loop for cooperative services
  /// 4. Call ShutdownServicesAsync() to cleanly shut down (returns boost::future<std::vector<std::exception_ptr>>)
  /// 5. Destructor automatically requests stop on all pending operations
  class LifecycleManager
  {
    LifecycleManagerConfig m_config;
    CooperativeThreadHost m_mainHost;
    std::vector<ServiceRegistrationRecord> m_registrations;
    std::map<ServiceThreadGroupId, std::unique_ptr<ManagedThreadHost>> m_threadHosts;
    std::stop_source m_stopSource;

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

    ~LifecycleManager()
    {
      // Request cancellation of all pending async operations
      m_stopSource.request_stop();
    }

    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;
    LifecycleManager(LifecycleManager&&) = delete;
    LifecycleManager& operator=(LifecycleManager&&) = delete;

    /// @brief Starts all registered services in priority order (highest first).
    ///
    /// Services are grouped by thread group and started in parallel within each priority level.
    /// On failure, all successfully started services are rolled back in reverse priority order,
    /// and an AggregateException is thrown containing all errors.
    ///
    /// @return Future that completes when all services are started.
    /// @throws AggregateException if any service fails to start (after rollback).
    boost::future<void> StartServicesAsync()
    {
      if (m_registrations.empty())
      {
        return boost::make_ready_future();
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

      // Start with a ready future and chain each priority level sequentially
      boost::future<void> chainedFuture = boost::make_ready_future();

      // Capture this and stop_token for continuations
      auto* self = this;
      auto stopToken = m_stopSource.get_token();

      // Start services in priority order (highest first due to std::greater comparator)
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
            // Chain the startup of this priority/threadGroup onto the previous future
            chainedFuture = chainedFuture.then(
              [self, stopToken, threadGroupId, priority,
               servicesForGroup = std::move(servicesForGroup)](boost::future<void> prevFuture) mutable -> boost::future<void>
              {
                // First, ensure previous stage completed successfully
                try
                {
                  prevFuture.get();    // Propagate any previous errors
                }
                catch (...)
                {
                  // If previous stage failed, don't start this stage - just propagate the exception
                  return boost::make_exceptional_future<void>(std::current_exception());
                }

                // Check if cancellation was requested
                if (stopToken.stop_requested())
                {
                  return boost::make_exceptional_future<void>(std::runtime_error("Operation cancelled"));
                }

                // Now start services for this priority/threadGroup
                boost::future<void> startFuture;

                try
                {
                  if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
                  {
                    // Main thread group - use cooperative host
                    startFuture = self->m_mainHost.GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
                  }
                  else
                  {
                    // Non-main thread group - ensure ManagedThreadHost exists and start it
                    auto it = self->m_threadHosts.find(threadGroupId);
                    if (it == self->m_threadHosts.end())
                    {
                      auto host = std::make_unique<ManagedThreadHost>(self->m_mainHost.GetExecutorContext());

                      // Start the thread (it will run io_context.run())
                      // Chain: start thread -> then start services
                      startFuture = host->StartAsync().then(
                        [self, stopToken, threadGroupId, priority, servicesForGroup = std::move(servicesForGroup),
                         host = std::move(host)](boost::future<ManagedThreadRecord> threadStartFuture) mutable -> boost::future<void>
                        {
                          try
                          {
                            threadStartFuture.get();    // Ensure thread started successfully

                            if (stopToken.stop_requested())
                            {
                              return boost::make_exceptional_future<void>(std::runtime_error("Operation cancelled"));
                            }

                            // Insert the host into our map
                            auto insertResult = self->m_threadHosts.emplace(threadGroupId, std::move(host));

                            // Start services on the managed thread host
                            return insertResult.first->second->GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
                          }
                          catch (...)
                          {
                            return boost::make_exceptional_future<void>(std::current_exception());
                          }
                        });
                    }
                    else
                    {
                      // Host already exists, just start services
                      startFuture = it->second->GetServiceHost()->TryStartServicesAsync(std::move(servicesForGroup), priority);
                    }
                  }

                  // Track successful startup after the future completes
                  return startFuture.then(
                    [self, stopToken, priority, threadGroupId](boost::future<void> serviceFuture) -> void
                    {
                      serviceFuture.get();    // Propagate any exceptions

                      if (!stopToken.stop_requested())
                      {
                        self->m_startedPriorities.push_back({priority, threadGroupId});
                      }
                    });
                }
                catch (...)
                {
                  return boost::make_exceptional_future<void>(std::current_exception());
                }
              });
          }
        }
      }

      // Handle any startup failures by triggering rollback
      return chainedFuture.then(
        [self, stopToken](boost::future<void> finalFuture) -> boost::future<void>
        {
          try
          {
            finalFuture.get();
            return boost::make_ready_future();
          }
          catch (...)
          {
            std::exception_ptr startupException = std::current_exception();

            if (stopToken.stop_requested())
            {
              // If cancelled, just propagate the original error
              return boost::make_exceptional_future<void>(startupException);
            }

            // Rollback all previously started priority levels
            return self->ShutdownServicesAsync().then(
              [startupException](boost::future<std::vector<std::exception_ptr>> shutdownFuture) -> void
              {
                std::vector<std::exception_ptr> rollbackErrors;
                try
                {
                  rollbackErrors = shutdownFuture.get();
                }
                catch (...)
                {
                  // If shutdown itself threw, capture that too
                  rollbackErrors.push_back(std::current_exception());
                }

                // Combine startup error with any rollback errors
                std::vector<std::exception_ptr> allErrors;
                allErrors.push_back(startupException);
                allErrors.insert(allErrors.end(), rollbackErrors.begin(), rollbackErrors.end());

                throw Common::AggregateException("Service startup failed", std::move(allErrors));
              });
          }
        });
    }

    /// @brief Shuts down all started services in reverse priority order.
    ///
    /// Iterates through started priority levels in reverse order and shuts down
    /// each one by calling TryShutdownServicesAsync on the appropriate host.
    /// After all services are stopped, managed threads are also shut down.
    ///
    /// @return Future containing a vector of any exceptions that occurred during shutdown.
    boost::future<std::vector<std::exception_ptr>> ShutdownServicesAsync()
    {
      auto allErrors = std::make_shared<std::vector<std::exception_ptr>>();
      auto* self = this;
      auto stopToken = m_stopSource.get_token();

      // Start with a ready future
      boost::future<void> chainedFuture = boost::make_ready_future();

      // Shutdown in reverse order of startup (lowest priority first, then higher)
      for (auto it = m_startedPriorities.rbegin(); it != m_startedPriorities.rend(); ++it)
      {
        auto priority = it->Priority;
        auto threadGroupId = it->ThreadGroupId;

        chainedFuture = chainedFuture.then(
          [self, stopToken, allErrors, priority, threadGroupId](boost::future<void> prevFuture) -> boost::future<void>
          {
            // Don't propagate previous errors - we want to continue shutting down
            try
            {
              prevFuture.get();
            }
            catch (...)
            {
              allErrors->push_back(std::current_exception());
            }

            if (stopToken.stop_requested())
            {
              allErrors->push_back(std::make_exception_ptr(std::runtime_error("Shutdown cancelled")));
              return boost::make_ready_future();
            }

            // Shutdown services at this priority level
            boost::future<std::vector<std::exception_ptr>> shutdownFuture;

            if (threadGroupId == ThreadGroupConfig::MainThreadGroupId)
            {
              shutdownFuture = self->m_mainHost.GetServiceHost()->TryShutdownServicesAsync(priority);
            }
            else
            {
              auto hostIt = self->m_threadHosts.find(threadGroupId);
              if (hostIt != self->m_threadHosts.end())
              {
                shutdownFuture = hostIt->second->GetServiceHost()->TryShutdownServicesAsync(priority);
              }
              else
              {
                // No host found - return empty errors
                shutdownFuture = boost::make_ready_future(std::vector<std::exception_ptr>());
              }
            }

            return shutdownFuture.then(
              [allErrors](boost::future<std::vector<std::exception_ptr>> errorsFuture) -> void
              {
                try
                {
                  auto errors = errorsFuture.get();
                  allErrors->insert(allErrors->end(), errors.begin(), errors.end());
                }
                catch (...)
                {
                  allErrors->push_back(std::current_exception());
                }
              });
          });
      }

      // After all services are shut down, clear the tracking and shutdown managed threads
      return chainedFuture.then(
        [self, stopToken, allErrors](boost::future<void> prevFuture) -> boost::future<std::vector<std::exception_ptr>>
        {
          // Don't propagate previous errors
          try
          {
            prevFuture.get();
          }
          catch (...)
          {
            allErrors->push_back(std::current_exception());
          }

          if (stopToken.stop_requested())
          {
            allErrors->push_back(std::make_exception_ptr(std::runtime_error("Thread shutdown cancelled")));
            return boost::make_ready_future(*allErrors);
          }

          // Clear the tracking
          self->m_startedPriorities.clear();

          // Shutdown all managed threads sequentially
          boost::future<void> threadShutdownChain = boost::make_ready_future();

          for (auto& [threadGroupId, host] : self->m_threadHosts)
          {
            threadShutdownChain = threadShutdownChain.then(
              [allErrors, &host](boost::future<void> prevFuture) -> boost::future<void>
              {
                // Don't propagate previous errors
                try
                {
                  prevFuture.get();
                }
                catch (...)
                {
                  allErrors->push_back(std::current_exception());
                }

                return host->TryShutdownAsync().then(
                  [allErrors](boost::future<bool> shutdownFuture) -> void
                  {
                    try
                    {
                      shutdownFuture.get();
                    }
                    catch (...)
                    {
                      allErrors->push_back(std::current_exception());
                    }
                  });
              });
          }

          return threadShutdownChain.then(
            [allErrors](boost::future<void> finalFuture) -> std::vector<std::exception_ptr>
            {
              try
              {
                finalFuture.get();
              }
              catch (...)
              {
                allErrors->push_back(std::current_exception());
              }
              return *allErrors;
            });
        });
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

    /// @brief Gets the stop token for checking cancellation status.
    ///
    /// @return Stop token for this lifecycle manager.
    std::stop_token GetStopToken() const
    {
      return m_stopSource.get_token();
    }

    /// @brief Requests cancellation of all pending async operations.
    ///
    /// This is automatically called by the destructor.
    void RequestStop()
    {
      m_stopSource.request_stop();
    }
  };
}

#endif
