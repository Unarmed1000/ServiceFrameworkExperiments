#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICEREGISTRY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICEREGISTRY_HPP
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

#include <Test2/Framework/Exception/ServiceRegistryException.hpp>
#include <Test2/Framework/Registry/IServiceRegistry.hpp>
#include <Test2/Framework/Registry/ServiceRegistrationRecord.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <spdlog/spdlog.h>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Test2
{
  /// @brief Concrete implementation of IServiceRegistry for managing service factory registration.
  ///
  /// ServiceRegistry maintains a collection of service factories along with their launch priorities
  /// and thread group assignments. Each factory type can only be registered once, identified by
  /// its runtime type information (typeid). After registration, factories can be extracted as an
  /// immutable collection via ExtractRegistrations(), which transfers ownership to the caller.
  ///
  /// The registry enforces one-time-use semantics: once ExtractRegistrations() is called, no new
  /// factories can be registered, though thread group IDs can still be generated.
  class ServiceRegistry : public IServiceRegistry
  {
  private:
    /// @brief Map of factory type to registration record.
    /// Key: std::type_index of the factory class type (typeid(*factory))
    /// Value: ServiceRegistrationRecord containing the factory and its metadata
    std::unordered_map<std::type_index, ServiceRegistrationRecord> m_registrations;

    /// @brief Counter for generating unique thread group identifiers.
    uint32_t m_nextThreadGroupId{1};

    /// @brief Flag indicating whether ExtractRegistrations() has been called.
    bool m_extracted{false};

  public:
    ServiceRegistry() = default;
    ~ServiceRegistry() override = default;

    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
    ServiceRegistry(ServiceRegistry&&) = delete;
    ServiceRegistry& operator=(ServiceRegistry&&) = delete;

    /// @brief Registers a service factory with the framework.
    ///
    /// This method validates and registers a service factory with the specified launch priority
    /// and thread group. The factory type (typeid) is used as the unique key - each factory
    /// type can only be registered once.
    ///
    /// @param factory Unique pointer to the service factory. Ownership is transferred to the registry.
    /// @param priority The launch priority determining initialization order (higher values first).
    /// @param threadGroupId The thread group identifier for this service's execution context.
    ///
    /// @throws InvalidServiceFactoryException if factory is null or reports zero supported interfaces
    /// @throws RegistryExtractedException if ExtractRegistrations() has already been called
    /// @throws DuplicateServiceRegistrationException if this factory type is already registered
    void RegisterService(std::unique_ptr<IServiceFactory> factory, const ServiceLaunchPriority priority,
                         const ServiceThreadGroupId threadGroupId) override
    {
      // Validate factory is not null
      if (!factory)
      {
        spdlog::error("ServiceRegistry::RegisterService: factory is null");
        throw InvalidServiceFactoryException("Cannot register null service factory");
      }

      // Check if registry has been extracted
      if (m_extracted)
      {
        spdlog::error("ServiceRegistry::RegisterService: registry has already been extracted");
        throw RegistryExtractedException("Cannot register services after ExtractRegistrations() has been called");
      }

      // Validate factory reports at least one supported interface
      auto interfaces = factory->GetSupportedInterfaces();
      if (interfaces.empty())
      {
        spdlog::error("ServiceRegistry::RegisterService: factory reports zero supported interfaces");
        throw InvalidServiceFactoryException("Service factory must support at least one interface");
      }

      // Get the factory type for duplicate detection
      const std::type_index factoryType(typeid(*factory));

      // Check if this factory type is already registered
      if (m_registrations.find(factoryType) != m_registrations.end())
      {
        spdlog::error("ServiceRegistry::RegisterService: factory type '{}' is already registered", factoryType.name());
        throw DuplicateServiceRegistrationException(fmt::format("Factory type '{}' is already registered", factoryType.name()));
      }

      // Register the factory
      spdlog::debug("ServiceRegistry::RegisterService: registering factory type '{}' with priority {} and thread group {}", factoryType.name(),
                    priority.GetValue(), threadGroupId.GetValue());

      m_registrations.emplace(factoryType, ServiceRegistrationRecord(std::move(factory), priority, threadGroupId));
    }

    /// @brief Creates a new unique service thread group identifier.
    ///
    /// Thread groups allow services to be organized into execution contexts. This method
    /// generates monotonically increasing unique identifiers starting from 1.
    ///
    /// @return A new unique ServiceThreadGroupId for organizing services.
    ServiceThreadGroupId CreateServiceThreadGroupId() override
    {
      const uint32_t groupId = m_nextThreadGroupId++;
      spdlog::debug("ServiceRegistry::CreateServiceThreadGroupId: created thread group ID {}", groupId);
      return ServiceThreadGroupId(groupId);
    }

    /// @brief Retrieves the main service thread group identifier.
    ///
    /// The main thread group is the primary execution context for services that need to run
    /// on the main application thread. This returns a reserved thread group ID (0) that is
    /// distinct from dynamically created thread groups (which start from 1).
    ///
    /// @return The ServiceThreadGroupId for the main thread group.
    ServiceThreadGroupId GetMainServiceThreadGroupId() override
    {
      spdlog::debug("ServiceRegistry::GetMainServiceThreadGroupId: returning main thread group ID 0");
      return ServiceThreadGroupId(0);
    }

    /// @brief Extracts all registered service factories and their metadata.
    ///
    /// This method transfers ownership of all registered factories to the caller as a vector
    /// of ServiceRegistrationRecord objects. After calling this method, the registry is marked
    /// as extracted and no further registrations are allowed.
    ///
    /// Subsequent calls to this method will return an empty vector.
    ///
    /// @return A vector of ServiceRegistrationRecord objects containing all registered factories.
    ///         Factory ownership is transferred to the caller.
    std::vector<ServiceRegistrationRecord> ExtractRegistrations()
    {
      spdlog::debug("ServiceRegistry::ExtractRegistrations: extracting {} registrations", m_registrations.size());

      // Mark as extracted
      m_extracted = true;

      // Build result vector by moving records
      std::vector<ServiceRegistrationRecord> result;
      result.reserve(m_registrations.size());

      for (auto& pair : m_registrations)
      {
        result.push_back(std::move(pair.second));
      }

      // Clear the map
      m_registrations.clear();

      spdlog::debug("ServiceRegistry::ExtractRegistrations: extracted {} registrations", result.size());
      return result;
    }
  };

}

#endif
