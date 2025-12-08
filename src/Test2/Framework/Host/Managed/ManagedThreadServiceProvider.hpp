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
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Test2
{
  class ManagedThreadServiceProvider : public IServiceProvider
  {
  public:
    /// @brief Represents a group of services at a specific priority level.
    ///
    /// Services within a priority group are stored in the order they were registered,
    /// enabling reverse-order shutdown within the priority level.
    struct PriorityGroup
    {
      ServiceLaunchPriority Priority;
      std::vector<ServiceInstanceInfo> Services;
    };

  private:
    std::vector<PriorityGroup> m_priorityGroups;
    std::unordered_multimap<std::type_index, std::shared_ptr<IServiceControl>> m_servicesByType;
    std::thread::id m_ownerThreadId;

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
    /// @brief Registers a priority group of services.
    ///
    /// Priority groups must be registered in strictly decreasing priority order.
    /// Each subsequent call must provide a priority value strictly less than the
    /// previously registered priority.
    ///
    /// Each ServiceInstanceInfo must contain a valid service and at least one supported interface.
    ///
    /// @param priority The priority level for this group of services.
    /// @param services The service instance info structs to register (will be moved).
    /// @throws EmptyPriorityGroupException if the services vector is empty.
    /// @throws InvalidPriorityOrderException if priority >= last registered priority.
    /// @throws std::invalid_argument if any service has no supported interfaces or null service pointer.
    void RegisterPriorityGroup(ServiceLaunchPriority priority, std::vector<Test2::ServiceInstanceInfo>&& services)
    {
      if (services.empty())
      {
        throw EmptyPriorityGroupException(fmt::format("Cannot register empty priority group for priority {}", priority.GetValue()));
      }

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

      // Validate each service and build type index
      for (size_t i = 0; i < services.size(); ++i)
      {
        if (!services[i].Service)
        {
          throw std::invalid_argument(fmt::format("Service at index {} has null service pointer", i));
        }
        if (services[i].SupportedInterfaces.empty())
        {
          throw std::invalid_argument(fmt::format("Service at index {} has no supported interfaces", i));
        }

        // Index service by each supported interface type
        for (const std::type_index& typeIndex : services[i].SupportedInterfaces)
        {
          m_servicesByType.emplace(typeIndex, services[i].Service);
        }
      }

      m_priorityGroups.emplace_back(PriorityGroup{priority, std::move(services)});
    }

    /// @brief Unregisters services at a specific priority level.
    ///
    /// Removes the priority group from the provider and returns the services.
    /// Services are removed from the type index as well.
    ///
    /// @param priority The priority level to unregister.
    /// @return The services that were at that priority level, or empty if not found.
    [[nodiscard]] std::vector<ServiceInstanceInfo> UnregisterPriorityGroup(ServiceLaunchPriority priority)
    {
      // Find the priority group
      auto it =
        std::find_if(m_priorityGroups.begin(), m_priorityGroups.end(), [priority](const PriorityGroup& group) { return group.Priority == priority; });

      if (it == m_priorityGroups.end())
      {
        return {};
      }

      // Remove services from type index
      for (const auto& info : it->Services)
      {
        for (const auto& typeIndex : info.SupportedInterfaces)
        {
          // Find and erase the specific service for this type
          auto range = m_servicesByType.equal_range(typeIndex);
          for (auto typeIt = range.first; typeIt != range.second; ++typeIt)
          {
            if (typeIt->second == info.Service)
            {
              m_servicesByType.erase(typeIt);
              break;
            }
          }
        }
      }

      // Move services out and remove the priority group
      std::vector<ServiceInstanceInfo> result = std::move(it->Services);
      m_priorityGroups.erase(it);
      return result;
    }

    // IServiceProvider interface implementations
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

    /// @brief Get the total count of registered services.
    ///
    /// Validates thread access and logs a warning if called from wrong thread.
    ///
    /// @return The total number of services across all priority groups, or 0 if called from wrong thread.
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
        count += group.Services.size();
      }
      return count;
    }

    /// @brief Get all registered service controls.
    ///
    /// Returns all unique IServiceControl instances in registration order.
    /// This is useful for iterating over all services, e.g., for processing.
    ///
    /// @return Vector of all service controls in registration order.
    [[nodiscard]] std::vector<std::shared_ptr<IServiceControl>> GetAllServiceControls() const
    {
      ValidateThreadAccess();
      std::vector<std::shared_ptr<IServiceControl>> result;

      for (const auto& group : m_priorityGroups)
      {
        for (const auto& info : group.Services)
        {
          result.push_back(info.Service);
        }
      }

      return result;
    }
  };
}

#endif