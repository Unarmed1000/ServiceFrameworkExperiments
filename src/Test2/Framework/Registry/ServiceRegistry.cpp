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

#include <Test2/Framework/Exception/DuplicateServiceRegistrationException.hpp>
#include <Test2/Framework/Exception/InvalidServiceFactoryException.hpp>
#include <Test2/Framework/Exception/RegistryExtractedException.hpp>
#include <Test2/Framework/Registry/ServiceRegistry.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <typeindex>

namespace Test2
{
  void ServiceRegistry::RegisterService(std::unique_ptr<IServiceFactory> factory, const ServiceLaunchPriority priority,
                                        const ServiceThreadGroupId threadGroupId)
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

  ServiceThreadGroupId ServiceRegistry::CreateServiceThreadGroupId()
  {
    const uint32_t groupId = m_nextThreadGroupId++;
    spdlog::debug("ServiceRegistry::CreateServiceThreadGroupId: created thread group ID {}", groupId);
    return ServiceThreadGroupId(groupId);
  }

  ServiceThreadGroupId ServiceRegistry::GetMainServiceThreadGroupId()
  {
    spdlog::debug("ServiceRegistry::GetMainServiceThreadGroupId: returning main thread group ID 0");
    return ServiceThreadGroupId(0);
  }

  std::vector<ServiceRegistrationRecord> ServiceRegistry::ExtractRegistrations()
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
}
