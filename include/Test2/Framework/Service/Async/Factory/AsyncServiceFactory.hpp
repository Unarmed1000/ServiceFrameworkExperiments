#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_FACTORY_ASYNCSERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_FACTORY_ASYNCSERVICEFACTORY_HPP
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

#include <Test2/Framework/Service/Async/Factory/IAsyncServiceFactory.hpp>
#include <memory>

namespace Test2
{
  /// @brief Abstract base class for async service factories.
  /// Concrete service factories should inherit from this class and implement both CreateProxy and Create methods.
  class AsyncServiceFactory : public IAsyncServiceFactory
  {
  protected:
    const std::type_index m_supportedInterface;

  public:
    explicit AsyncServiceFactory(const std::type_index supportedInterface)
      : m_supportedInterface(supportedInterface)
    {
    }

    virtual ~AsyncServiceFactory() = default;

    std::span<const std::type_index> GetSupportedInterfaces() const final
    {
      return std::span<const std::type_index>(&m_supportedInterface, 1);
    }

    std::type_index GetImplFactoryTypeId() const final
    {
      return std::type_index(typeid(*this));
    }

    // Pure virtual methods for derived classes to implement
    virtual std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& createInfo) = 0;
    virtual std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& createInfo) = 0;
  };

}

#endif