#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADSERVICEPROVIDER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADSERVICEPROVIDER_HPP
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

#include <Test2/Framework/Exception/EmptyPriorityGroupException.hpp>
#include <Test2/Framework/Exception/InvalidPriorityOrderException.hpp>
#include <Test2/Framework/Exception/MultipleServicesFoundException.hpp>
#include <Test2/Framework/Exception/ServiceProviderException.hpp>
#include <Test2/Framework/Exception/UnknownServiceException.hpp>
#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Provider/IServiceProvider.hpp>
#include <Test2/Framework/Provider/ServiceProviderServiceInstance.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <Test2/Framework/Service/IServiceControlBase.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Test2
{
  /// @brief Thread-local service provider with staged registration for priority-based access control.
  ///
  /// This provider manages service and proxy lifetimes using a two-phase registration pattern:
  /// 1. STAGING: RegisterPriorityGroup() stages instances (not yet visible via GetService/TryGetService)
  /// 2. COMMIT: CommitStagedPriority() makes staged instances visible to dependency lookups
  ///
  /// STAGING BEHAVIOR:
  /// - Instances are staged when RegisterPriorityGroup() is called
  /// - Staged instances are NOT visible via GetService/TryGetService until committed
  /// - This ensures services at priority N cannot access other services at priority N during initialization
  /// - Services can only access higher-priority services (already committed)
  ///
  /// AUTO-COMMIT:
  /// - When RegisterPriorityGroup() is called with a different priority than currently staged,
  ///   the previously staged priority is automatically committed first
  /// - This simplifies the common case of registering multiple priorities sequentially
  ///
  /// LIFECYCLE:
  /// - Services must be registered before proxies at the same priority
  /// - Priority groups must be registered in strictly decreasing order
  /// - Call CommitStagedPriority() to make staged instances visible
  /// - Call DiscardStagedPriority() to rollback staging on initialization failure
  ///
  /// THREAD SAFETY:
  /// - All methods must be called from the owner thread (thread that constructed the provider)
  /// - Thread validation occurs on each method call
  ///
  /// @see CommitStagedPriority()
  /// @see DiscardStagedPriority()
  /// @see RegisterPriorityGroup()
  class ManagedThreadServiceProvider : public IServiceProvider
  {
  public:
    /// @brief Represents a group of services and/or proxies at a specific priority level.
    ///
    /// Instances within a priority group are stored in the order they were registered,
    /// with services always registered before proxies at the same priority level.
    /// This enables reverse-order shutdown within the priority level.
    struct PriorityGroup
    {
      ServiceLaunchPriority Priority;
      std::vector<ServiceProviderServiceInstance> Instances;
    };

  private:
    std::vector<PriorityGroup> m_priorityGroups;
    std::unordered_multimap<std::type_index, std::shared_ptr<IServiceControlBase>> m_servicesByType;
    std::thread::id m_ownerThreadId;

    /// @brief Staging area for instances before they are committed.
    /// Staged instances are not visible via GetService/TryGetService until CommitStagedPriority() is called.
    std::vector<ServiceProviderServiceInstance> m_stagedInstances;

    /// @brief Priority level of currently staged instances.
    ServiceLaunchPriority m_stagedPriority{0};

    /// @brief Flag indicating if staging is in error state (commit failed).
    /// When true, new registrations are blocked until proper shutdown occurs.
    bool m_stagingInErrorState{false};

    /// @brief Validates that the current thread is the owner thread.
    /// @throws ServiceProviderException if called from a different thread.
    void ValidateThreadAccess() const
    {
      const auto currentThreadId = std::this_thread::get_id();
      if (currentThreadId != m_ownerThreadId)
      {
        spdlog::error("ServiceProvider accessed from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
        throw ServiceProviderException("ServiceProvider accessed from wrong thread");
      }
    }

  public:
    ManagedThreadServiceProvider()
      : m_ownerThreadId(std::this_thread::get_id())
    {
    }
    /// @brief Registers a priority group of services or proxies.
    ///
    /// Priority groups must be registered in strictly decreasing priority order.
    /// Services must be registered before proxies at the same priority level.
    ///
    /// If the priority group doesn't exist, it will be created.
    /// If the priority group exists, instances will be appended (must follow service-before-proxy rule).
    ///
    /// Each instance must have a valid pointer and at least one supported interface.
    ///
    /// AUTO-COMMIT BEHAVIOR: If there are staged instances at a different priority level,
    /// they will be automatically committed before staging the new instances.
    ///
    /// @param instanceType Whether these are services or proxies.
    /// @param priority The priority level for this group.
    /// @param instances The instance info structs to register (will be moved).
    /// @throws EmptyPriorityGroupException if the instances vector is empty.
    /// @throws InvalidPriorityOrderException if priority ordering is violated or service-before-proxy rule violated.
    /// @throws std::invalid_argument if any instance has no supported interfaces or null pointer.
    /// @throws ServiceProviderException if staging is in error state (must shutdown before new registrations).
    void RegisterPriorityGroup(InstanceType instanceType, ServiceLaunchPriority priority, std::vector<ServiceProviderServiceInstance>&& instances)
    {
      if (instances.empty())
      {
        throw EmptyPriorityGroupException(fmt::format("Cannot register empty priority group for priority {}", priority.GetValue()));
      }

      // Check if staging is in error state - must shutdown before new registrations
      if (m_stagingInErrorState)
      {
        throw ServiceProviderException(
          fmt::format("Cannot register new instances - staging is in error state. "
                      "Must call TryShutdownServiceProxiesAsync + TryShutdownServicesAsync before new registrations."));
      }

      // AUTO-COMMIT: If staging a different priority, commit the current staged priority first
      if (!m_stagedInstances.empty() && m_stagedPriority != priority)
      {
        CommitStagedPriority();
      }

      // Validate all instances have correct type
      for (size_t i = 0; i < instances.size(); ++i)
      {
        if (instances[i].Type != instanceType)
        {
          throw std::invalid_argument(fmt::format("Instance at index {} has type mismatch. Expected {}, got {}", i,
                                                  instanceType == InstanceType::Service ? "Service" : "Proxy",
                                                  instances[i].Type == InstanceType::Service ? "Service" : "Proxy"));
        }
      }

      // Find existing priority group
      auto it =
        std::find_if(m_priorityGroups.begin(), m_priorityGroups.end(), [priority](const PriorityGroup& group) { return group.Priority == priority; });

      if (it != m_priorityGroups.end())
      {
        // Priority group exists - check if this instance type was already registered
        bool hasInstancesOfType = false;
        for (const auto& instance : it->Instances)
        {
          if (instance.Type == instanceType)
          {
            hasInstancesOfType = true;
            break;
          }
        }

        if (hasInstancesOfType)
        {
          throw InvalidPriorityOrderException(
            fmt::format("Cannot register {} instances at priority {} multiple times. Each instance type can only be registered once per priority.",
                        instanceType == InstanceType::Service ? "service" : "proxy", priority.GetValue()));
        }

        // Validate service-before-proxy ordering
        if (!it->Instances.empty())
        {
          const auto lastInstanceType = it->Instances.back().Type;
          if (lastInstanceType == InstanceType::Proxy && instanceType == InstanceType::Service)
          {
            throw InvalidPriorityOrderException(
              fmt::format("Cannot register services at priority {} after proxies have already been registered. "
                          "Services must be registered before proxies at the same priority level.",
                          priority.GetValue()));
          }
        }

        // Validate instances before staging
        for (size_t i = 0; i < instances.size(); ++i)
        {
          if (!instances[i].Instance)
          {
            throw std::invalid_argument(fmt::format("Instance at index {} has null instance pointer", i));
          }
          if (instances[i].SupportedInterfaces.empty())
          {
            throw std::invalid_argument(fmt::format("Instance at index {} has no supported interfaces", i));
          }
        }

        // Stage instances instead of immediately committing
        m_stagedPriority = priority;
        m_stagedInstances.insert(m_stagedInstances.end(), std::make_move_iterator(instances.begin()), std::make_move_iterator(instances.end()));
      }
      else
      {
        // New priority group - validate decreasing priority order
        if (!m_priorityGroups.empty())
        {
          const auto lastPriority = m_priorityGroups.back().Priority;
          if (priority >= lastPriority)
          {
            throw InvalidPriorityOrderException(
              fmt::format("Priority order violation: attempting to register priority {} after priority {}. "
                          "Priority groups must be registered in strictly decreasing order (high to low).",
                          priority.GetValue(), lastPriority.GetValue()));
          }
        }

        // Validate instances before staging
        for (size_t i = 0; i < instances.size(); ++i)
        {
          if (!instances[i].Instance)
          {
            throw std::invalid_argument(fmt::format("Instance at index {} has null instance pointer", i));
          }
          if (instances[i].SupportedInterfaces.empty())
          {
            throw std::invalid_argument(fmt::format("Instance at index {} has no supported interfaces", i));
          }
        }

        // Stage instances instead of immediately committing
        m_stagedPriority = priority;
        m_stagedInstances.insert(m_stagedInstances.end(), std::make_move_iterator(instances.begin()), std::make_move_iterator(instances.end()));
      }
    }

    /// @brief Commits all staged instances, making them visible via GetService/TryGetService.
    ///
    /// All staged instances must have the same priority value. This method will:
    /// 1. Create or append to the appropriate priority group
    /// 2. Index all instances by their supported interfaces
    /// 3. Clear the staging area
    ///
    /// If this method throws an exception, staging enters error state and new registrations
    /// are blocked until proper shutdown (TryShutdownServiceProxiesAsync + TryShutdownServicesAsync).
    ///
    /// @throws std::runtime_error if staged instances have mismatched priorities
    void CommitStagedPriority()
    {
      ValidateThreadAccess();

      if (m_stagedInstances.empty())
      {
        return;
      }

      try
      {
        // Find or create priority group
        auto it = std::find_if(m_priorityGroups.begin(), m_priorityGroups.end(),
                               [this](const PriorityGroup& group) { return group.Priority == m_stagedPriority; });

        if (it != m_priorityGroups.end())
        {
          // Priority group exists - append staged instances
          size_t oldSize = it->Instances.size();
          it->Instances.insert(it->Instances.end(), std::make_move_iterator(m_stagedInstances.begin()),
                               std::make_move_iterator(m_stagedInstances.end()));

          // Index newly added instances
          for (size_t i = oldSize; i < it->Instances.size(); ++i)
          {
            for (const std::type_index& typeIndex : it->Instances[i].SupportedInterfaces)
            {
              m_servicesByType.emplace(typeIndex, it->Instances[i].Instance);
            }
          }
        }
        else
        {
          // Create new priority group with moved instances
          PriorityGroup newGroup{m_stagedPriority, std::move(m_stagedInstances)};

          // Index all instances in the new group
          for (const auto& instance : newGroup.Instances)
          {
            for (const std::type_index& typeIndex : instance.SupportedInterfaces)
            {
              m_servicesByType.emplace(typeIndex, instance.Instance);
            }
          }

          m_priorityGroups.emplace_back(std::move(newGroup));
        }

        // Clear staging
        m_stagedInstances.clear();
        m_stagedPriority = ServiceLaunchPriority{0};
      }
      catch (...)
      {
        // Mark staging as in error state - new registrations blocked until shutdown
        m_stagingInErrorState = true;
        spdlog::error("CommitStagedPriority failed - staging now in error state. Must shutdown before new registrations.");
        throw;
      }
    }

    /// @brief Discards staged instances of the specified type without committing them.
    ///
    /// Removes only staged instances matching the specified type from the staging area.
    /// This is typically called during shutdown to cleanup uncommitted instances.
    ///
    /// @param type The instance type to discard (Proxy or Service)
    ///
    /// IDEMPOTENT: Safe to call multiple times or when staging is already empty.
    /// No-op if there are no staged instances of the specified type.
    ///
    /// Clears error state if ALL staged instances are discarded.
    ///
    /// @note Does not perform thread validation - safe to call during cleanup/exception handling.
    void DiscardStagedPriority(InstanceType type) noexcept
    {
      // No thread validation - this may be called during cleanup

      // Remove instances matching the specified type
      auto it = std::remove_if(m_stagedInstances.begin(), m_stagedInstances.end(),
                               [type](const ServiceProviderServiceInstance& instance) { return instance.Type == type; });

      const size_t removedCount = std::distance(it, m_stagedInstances.end());
      m_stagedInstances.erase(it, m_stagedInstances.end());

      // If all staged instances are now gone, clear staging completely
      if (m_stagedInstances.empty())
      {
        m_stagedPriority = ServiceLaunchPriority(0);
        m_stagingInErrorState = false;    // Clear error state - cleanup complete
        spdlog::info("DiscardStagedPriority({}) - all staged instances cleared, error state reset",
                     type == InstanceType::Proxy ? "Proxy" : "Service");
      }
      else
      {
        spdlog::info("DiscardStagedPriority({}) - removed {} instances, {} remaining", type == InstanceType::Proxy ? "Proxy" : "Service",
                     removedCount, m_stagedInstances.size());
      }
    }

    /// @brief Unregisters instances of a specific type at a specific priority level.
    ///
    /// Removes all instances matching the specified type from the priority group.
    /// Instances are removed from the type index as well.
    /// If the priority group becomes empty, it is removed entirely.
    ///
    /// @param instanceType Whether to unregister services or proxies.
    /// @param priority The priority level to unregister from.
    /// @return The instances that were unregistered, or empty if none found.
    [[nodiscard]] std::vector<ServiceProviderServiceInstance> UnregisterPriorityGroup(InstanceType instanceType, ServiceLaunchPriority priority)
    {
      // Find the priority group
      auto it =
        std::find_if(m_priorityGroups.begin(), m_priorityGroups.end(), [priority](const PriorityGroup& group) { return group.Priority == priority; });

      if (it == m_priorityGroups.end())
      {
        return {};
      }

      std::vector<ServiceProviderServiceInstance> result;

      // Partition instances: matching type goes to end
      auto partitionPoint = std::partition(it->Instances.begin(), it->Instances.end(),
                                           [instanceType](const ServiceProviderServiceInstance& instance) { return instance.Type != instanceType; });

      // Move matching instances to result and remove from type index
      for (auto instanceIt = partitionPoint; instanceIt != it->Instances.end(); ++instanceIt)
      {
        // Remove from type index
        for (const auto& typeIndex : instanceIt->SupportedInterfaces)
        {
          auto range = m_servicesByType.equal_range(typeIndex);
          for (auto typeIt = range.first; typeIt != range.second; ++typeIt)
          {
            if (typeIt->second == instanceIt->Instance)
            {
              m_servicesByType.erase(typeIt);
              break;
            }
          }
        }

        result.push_back(std::move(*instanceIt));
      }

      // Erase the moved instances
      it->Instances.erase(partitionPoint, it->Instances.end());

      // If priority group is now empty, remove it
      if (it->Instances.empty())
      {
        m_priorityGroups.erase(it);
      }

      return result;
    }

    // IServiceProvider interface implementations
    // MISSING FEATURE: GetService does NOT enforce priority-based access restrictions.
    // TODO: This method should filter results based on the calling service's priority level.
    //       Services at priority N should only access services at priority > N (strictly higher).
    std::shared_ptr<IService> GetService(const std::type_info& type) const override
    {
      ValidateThreadAccess();
      const std::type_index typeIndex(type);
      auto range = m_servicesByType.equal_range(typeIndex);

      if (range.first == range.second)
      {
        throw UnknownServiceException(std::string("No service found for type: ") + type.name());
      }

      // Check if there's exactly one service
      auto it = range.first;
      auto next = it;
      ++next;

      if (next != range.second)
      {
        throw MultipleServicesFoundException(std::string("Multiple services found for type: ") + type.name() +
                                             ". Use TryGetServices to retrieve all matching services.");
      }

      return it->second;
    }

    std::shared_ptr<IService> TryGetService(const std::type_info& type) const override
    {
      ValidateThreadAccess();
      const std::type_index typeIndex(type);
      auto it = m_servicesByType.find(typeIndex);

      if (it == m_servicesByType.end())
      {
        return nullptr;
      }

      return it->second;
    }

    bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const override
    {
      ValidateThreadAccess();
      const std::type_index typeIndex(type);
      auto range = m_servicesByType.equal_range(typeIndex);

      if (range.first == range.second)
      {
        return false;
      }

      for (auto it = range.first; it != range.second; ++it)
      {
        rServices.push_back(it->second);
      }

      return true;
    }

    /// @brief Get the total count of registered instances (services and proxies).
    ///
    /// Validates thread access and logs a warning if called from wrong thread.
    ///
    /// @return The total number of instances across all priority groups, or 0 if called from wrong thread.
    [[nodiscard]] std::size_t GetServiceCount() const noexcept
    {
      const auto currentThreadId = std::this_thread::get_id();
      if (currentThreadId != m_ownerThreadId)
      {
        spdlog::warn("GetServiceCount called from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
        return 0;
      }

      std::size_t count = 0;
      for (const auto& group : m_priorityGroups)
      {
        count += group.Instances.size();
      }
      return count;
    }

    /// @brief Get the count of staged (uncommitted) instances.
    /// @return The number of instances currently staged but not yet committed.
    [[nodiscard]] std::size_t GetStagedServiceCount() const noexcept
    {
      const auto currentThreadId = std::this_thread::get_id();
      if (currentThreadId != m_ownerThreadId)
      {
        spdlog::warn("GetStagedServiceCount called from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
        return 0;
      }

      return m_stagedInstances.size();
    }

    /// @brief Get the count of staged instances of a specific type.
    /// @param type The instance type to count (Proxy or Service)
    /// @return The number of instances of the specified type currently staged.
    [[nodiscard]] std::size_t GetStagedInstanceCount(InstanceType type) const noexcept
    {
      const auto currentThreadId = std::this_thread::get_id();
      if (currentThreadId != m_ownerThreadId)
      {
        spdlog::warn("GetStagedInstanceCount called from wrong thread. Owner: {}, Caller: {}", m_ownerThreadId, currentThreadId);
        return 0;
      }

      return std::count_if(m_stagedInstances.begin(), m_stagedInstances.end(),
                           [type](const ServiceProviderServiceInstance& instance) { return instance.Type == type; });
    }

    /// @brief Process all registered instances (services and proxies).
    ///
    /// Iterates through all instances in registration order, calling Process() on each,
    /// and merges the results to determine the most restrictive outcome.
    ///
    /// @return Merged ProcessResult representing the most restrictive result from all instances.
    [[nodiscard]] ProcessResult Process()
    {
      ValidateThreadAccess();
      ProcessResult result = ProcessResult::NoSleepLimit();

      for (const auto& group : m_priorityGroups)
      {
        for (const auto& instance : group.Instances)
        {
          result = Merge(result, instance.Instance->Process());
        }
      }

      return result;
    }
  };
}

#endif