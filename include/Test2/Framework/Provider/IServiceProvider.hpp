#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_ISERVICEPROVIDER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_ISERVICEPROVIDER_HPP
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

#include <Test2/Framework/Service/IService.hpp>
#include <memory>
#include <typeinfo>
#include <vector>

namespace Test2
{
  /// @brief Interface for retrieving services from a service registry.
  ///
  /// The IServiceProvider interface provides methods to query and retrieve service instances
  /// from a service registry. It supports both single service retrieval and batch retrieval
  /// of multiple services of the same type.
  class IServiceProvider
  {
  public:
    virtual ~IServiceProvider() = default;

    /// @brief Retrieves a service instance of the specified type.
    ///
    /// This method retrieves a service that matches the given type information.
    /// If the service is not found, it throws an exception.
    ///
    /// @param type The type information of the service to retrieve.
    /// @return A shared pointer to the requested service instance.
    /// @throws UnknownServiceException if the service is not found
    virtual std::shared_ptr<IService> GetService(const std::type_info& type) const = 0;

    /// @brief Attempts to retrieve a service instance of the specified type.
    ///
    /// This method attempts to retrieve a service that matches the given type information.
    /// Unlike GetService, this method does not throw if the service is not found;
    /// instead, it returns nullptr.
    ///
    /// @param type The type information of the service to retrieve.
    /// @return A shared pointer to the requested service instance, or nullptr if not found.
    virtual std::shared_ptr<IService> TryGetService(const std::type_info& type) const = 0;

    /// @brief Attempts to retrieve all service instances of the specified type.
    ///
    /// This method retrieves all services that match the given type information.
    /// The services are added to the provided vector.
    ///
    /// @param type The type information of the services to retrieve.
    /// @param rServices Reference to a vector that will be populated with the matching service instances.
    /// @return true if one or more services were found and added to rServices, false otherwise.
    virtual bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const = 0;
  };

}

#endif