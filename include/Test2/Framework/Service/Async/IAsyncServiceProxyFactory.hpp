#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_IASYNCSERVICEPROXYFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_IASYNCSERVICEPROXYFACTORY_HPP
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

#include <Test2/Framework/Service/IServiceFactoryInfo.hpp>
#include <Test2/Framework/Service/IServiceProxyControl.hpp>
#include <memory>
#include <typeindex>

namespace Test2
{
  struct ServiceProxyCreateInfo;

  class IAsyncServiceProxyFactory : public virtual IServiceFactoryInfo
  {
  public:
    virtual ~IAsyncServiceProxyFactory() = default;

    /// @brief Creates a new service proxy instance of the specified type.
    ///
    /// This method instantiates a proxy for a service that implements the requested interface type.
    /// Proxies enable services to interact with other services across different threading contexts
    /// or dispatch boundaries. The factory can expect that its only called with types returned by GetSupportedInterfaces().
    ///
    /// @param type The type index of the service interface to create a proxy for.
    /// @param createInfo Context information for proxy creation, including the service
    ///                   provider for accessing dependencies.
    /// @return A shared pointer to the newly created service proxy instance.
    /// @throws std::invalid_argument if the requested type is not supported by this factory.
    virtual std::shared_ptr<IServiceProxyControl> CreateProxy(const std::type_index& type, const ServiceProxyCreateInfo& createInfo) = 0;
  };

}

#endif