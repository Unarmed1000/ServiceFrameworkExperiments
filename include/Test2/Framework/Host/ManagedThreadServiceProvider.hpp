#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
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

#include <Test2/Framework/Host/EmptyPriorityGroupException.hpp>
#include <Test2/Framework/Host/InvalidPriorityOrderException.hpp>
#include <Test2/Framework/Provider/IServiceProvider.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <memory>
#include <vector>

namespace Test2
{
  class ManagedThreadServiceProvider : IServiceProvider
  {
  public:
    /// @brief Represents a group of services at a specific priority level.
    ///
    /// Services within a priority group are stored in the order they were registered,
    /// enabling reverse-order shutdown within the priority level.
    struct PriorityGroup
    {
      ServiceLaunchPriority Priority;
      std::vector<std::shared_ptr<IService>> Services;
    };

  private:
    std::vector<PriorityGroup> m_priorityGroups;

  public:
    /// @brief Registers a priority group of services.
    ///
    /// Priority groups must be registered in strictly decreasing priority order.
    /// Each subsequent call must provide a priority value strictly less than the
    /// previously registered priority.
    ///
    /// @param priority The priority level for this group of services.
    /// @param services The services to register at this priority level (will be moved).
    /// @throws EmptyPriorityGroupException if the services vector is empty.
    /// @throws InvalidPriorityOrderException if priority >= last registered priority.
    void RegisterPriorityGroup(ServiceLaunchPriority priority, std::vector<std::shared_ptr<IService>>&& services)
    {
      if (services.empty())
      {
        throw EmptyPriorityGroupException(std::string("Cannot register empty priority group for priority ") + std::to_string(priority.GetValue()));
      }

      if (!m_priorityGroups.empty())
      {
        const auto lastPriority = m_priorityGroups.back().Priority;
        if (priority >= lastPriority)
        {
          throw InvalidPriorityOrderException(std::string("Priority order violation: attempting to register priority ") +
                                              std::to_string(priority.GetValue()) + " after priority " + std::to_string(lastPriority.GetValue()) +
                                              ". Priority groups must be registered in strictly decreasing order (high to low).");
        }
      }

      m_priorityGroups.emplace_back(PriorityGroup{priority, std::move(services)});
    }

    /// @brief Unregisters all services in reverse priority order.
    ///
    /// Returns all priority groups in shutdown order (lowest to highest priority).
    /// The internal priority group list is cleared after this operation.
    ///
    /// @return A vector of priority groups in shutdown order (reverse of registration order).
    [[nodiscard]] std::vector<PriorityGroup> UnregisterAllServices()
    {
      std::vector<PriorityGroup> result;
      result.reserve(m_priorityGroups.size());

      // Iterate in reverse to get low-to-high priority order for shutdown
      for (auto it = m_priorityGroups.rbegin(); it != m_priorityGroups.rend(); ++it)
      {
        result.emplace_back(PriorityGroup{it->Priority, std::move(it->Services)});
      }

      m_priorityGroups.clear();
      return result;
    }

    // IServiceProvider interface stub implementations
    // These will be properly implemented when integrating with the service registry
    std::shared_ptr<IService> GetService(const std::type_info& /*type*/) const override
    {
      throw std::runtime_error("ManagedThreadServiceProvider::GetService not yet implemented");
    }

    std::shared_ptr<IService> TryGetService(const std::type_info& /*type*/) const override
    {
      return nullptr;
    }

    bool TryGetServices(const std::type_info& /*type*/, std::vector<std::shared_ptr<IService>>& /*rServices*/) const override
    {
      return false;
    }
  };
}

#endif