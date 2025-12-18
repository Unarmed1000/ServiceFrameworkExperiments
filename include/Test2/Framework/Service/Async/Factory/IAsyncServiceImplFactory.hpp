#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_FACTORY_IASYNCSERVICEIMPLFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_FACTORY_IASYNCSERVICEIMPLFACTORY_HPP
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

#include <Test2/Framework/Service/Async/Factory/IAsyncServiceProxyFactory.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <memory>
#include <span>
#include <typeindex>

namespace Test2
{
  struct ServiceCreateInfo;

  /// @brief Interface for creating service instances in the service framework.
  ///
  /// The IAsyncServiceImplFactory interface defines the contract for service factories that are
  /// responsible for creating service instances. Each factory knows which service interfaces
  /// it can produce and can create instances of those services on demand.
  ///
  /// Factories are registered with the IServiceRegistry during application initialization
  /// and are used internally by the framework to instantiate services when needed.
  class IAsyncServiceImplFactory : public virtual IServiceFactoryInfo
  {
  public:
    virtual ~IAsyncServiceImplFactory() = default;

    /// @brief Creates a new service instance of the specified type.
    ///
    /// This method instantiates a service that implements the requested interface type.
    /// The factory can expect that its only called with types returned by GetSupportedInterfaces()
    /// The createInfo parameter provides access to the service
    /// provider, allowing the new service to retrieve dependencies during construction.
    ///
    /// @param type The type index of the service interface to create.
    /// @return A shared pointer to the newly created service instance.
    /// @throws std::invalid_argument if the requested type is not supported by this factory.
    virtual std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& createInfo) = 0;
  };

}

#endif