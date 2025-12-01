#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICEREGISTRATIONRECORD_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICEREGISTRATIONRECORD_HPP
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

#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <memory>

namespace Test2
{
  /// @brief Record containing a registered service factory with its associated metadata.
  ///
  /// This structure holds a service factory along with its launch priority and thread group
  /// assignment. Ownership of the factory is transferred to the holder of this record.
  /// Records are typically obtained via ServiceRegistry::ExtractRegistrations().
  struct ServiceRegistrationRecord
  {
    /// @brief The service factory that creates service instances.
    /// Ownership is held by this record.
    std::unique_ptr<IServiceFactory> Factory;

    /// @brief The launch priority determining initialization order.
    /// Higher values launch first and can be accessed as dependencies by lower-priority services.
    ServiceLaunchPriority Priority;

    /// @brief The thread group identifier for this service's execution context.
    /// Services within the same thread group may share execution resources.
    ServiceThreadGroupId ThreadGroupId;

    /// @brief Default constructor.
    ServiceRegistrationRecord() = default;

    /// @brief Constructs a registration record with the specified factory, priority, and thread group.
    ///
    /// @param factory Unique pointer to the service factory (ownership transferred).
    /// @param priority The launch priority for services created by this factory.
    /// @param threadGroupId The thread group for services created by this factory.
    ServiceRegistrationRecord(std::unique_ptr<IServiceFactory> factory, ServiceLaunchPriority priority, ServiceThreadGroupId threadGroupId)
      : Factory(std::move(factory))
      , Priority(priority)
      , ThreadGroupId(threadGroupId)
    {
    }

    /// @brief Move constructor.
    ServiceRegistrationRecord(ServiceRegistrationRecord&&) noexcept = default;

    /// @brief Move assignment operator.
    ServiceRegistrationRecord& operator=(ServiceRegistrationRecord&&) noexcept = default;

    /// @brief Copy constructor is deleted (factory ownership is unique).
    ServiceRegistrationRecord(const ServiceRegistrationRecord&) = delete;

    /// @brief Copy assignment operator is deleted (factory ownership is unique).
    ServiceRegistrationRecord& operator=(const ServiceRegistrationRecord&) = delete;
  };

}

#endif
