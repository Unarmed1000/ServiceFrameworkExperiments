#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_ISERVICEREGISTRY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_ISERVICEREGISTRY_HPP
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
#include <memory>

namespace Test2
{
  class IServiceFactory;

  /// @brief Interface for registering services with the service framework.
  ///
  /// The IServiceRegistry interface provides methods to register services and manage
  /// service thread groups. Services are registered with a launch priority and assigned
  /// to a thread group, which determines execution ordering and threading behavior.
  class IServiceRegistry
  {
  public:
    virtual ~IServiceRegistry() = default;

    /// @brief Registers a service with the framework.
    ///
    /// This method registers a service factory with the specified launch priority and thread group.
    /// Services are launched in priority order during framework initialization, with higher
    /// priority values launching first. A service can only access other services with higher
    /// priority (higher numerical value), ensuring proper dependency ordering.
    ///
    /// @param factory Unique pointer to the service factory that will create service instances.
    ///                Ownership is transferred to the registry.
    /// @param priority The launch priority determining initialization order (higher values first).
    ///                 Also determines service dependency access - services can only depend on
    ///                 services with higher priority values.
    /// @param threadGroupId The thread group identifier for this service's execution context.
    ///                      Services within the same thread group may share execution resources.
    virtual void RegisterService(std::unique_ptr<IServiceFactory> factory, const ServiceLaunchPriority priority,
                                 const ServiceThreadGroupId threadGroupId) = 0;

    /// @brief Creates a new unique service thread group identifier.
    ///
    /// Thread groups allow services to be organized into execution contexts. Services within
    /// the same thread group may share execution resources or coordination mechanisms.
    ///
    /// @return A new unique ServiceThreadGroupId for organizing services.
    virtual ServiceThreadGroupId CreateServiceThreadGroupId() const = 0;
  };

}

#endif