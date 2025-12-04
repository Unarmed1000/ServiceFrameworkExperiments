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

#include <Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadHost.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Lifecycle/LifecycleManagerConfig.hpp>
#include <Test2/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <boost/asio/awaitable.hpp>
#include <map>
#include <memory>
#include <vector>

namespace Test2
{
  /// @brief Manages the lifecycle of services across multiple thread groups.
  ///
  /// LifecycleManager orchestrates service startup and shutdown across thread groups.
  /// It holds a CooperativeThreadServiceHost for the main thread group (ID 0) and spawns
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
    CooperativeThreadServiceHost m_mainHost;
    std::vector<ServiceRegistrationRecord> m_registrations;
    std::map<ServiceThreadGroupId, std::unique_ptr<ManagedThreadHost>> m_threadHosts;

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

      // Group registrations by priority
      std::map<ServiceLaunchPriority, std::vector<ServiceRegistrationRecord*>, std::greater<ServiceLaunchPriority>> priorityGroups;

      for (auto& reg : m_registrations)
      {
        priorityGroups[reg.Priority].push_back(&reg);
      }

      // Start services in priority order (highest first due to std::greater comparator)
      for (auto& [priority, regsInGroup] : priorityGroups)
      {
        std::vector<StartServiceRecord> servicesForPriority;

        for (auto* reg : regsInGroup)
        {
          // Get service name from first supported interface
          auto interfaces = reg->Factory->GetSupportedInterfaces();
          std::string serviceName = interfaces.empty() ? "UnknownService" : interfaces[0].name();

          servicesForPriority.emplace_back(std::move(serviceName), std::move(reg->Factory));
        }

        if (!servicesForPriority.empty())
        {
          co_await m_mainHost.TryStartServicesAsync(std::move(servicesForPriority), priority);
        }
      }

      co_return;
    }

    /// @brief Shuts down all services in reverse priority order.
    ///
    /// @return Awaitable that completes when all services are shut down.
    boost::asio::awaitable<void> ShutdownServicesAsync()
    {
      // TODO: Implement in Phase 6
      co_return;
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

    /// @brief Gets the main thread's service host.
    ///
    /// Use this to set the wake callback for cross-thread notification.
    ///
    /// @return Reference to the CooperativeThreadServiceHost for the main thread.
    CooperativeThreadServiceHost& GetMainHost()
    {
      return m_mainHost;
    }

    /// @brief Gets the main thread's service host (const version).
    const CooperativeThreadServiceHost& GetMainHost() const
    {
      return m_mainHost;
    }
  };
}

#endif