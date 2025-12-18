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

#include <Test2/Framework/Service/Async/AsyncServiceFactory.hpp>
#include <Test2/Framework/Service/Async/IAsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/IAsyncServiceProxyFactory.hpp>


namespace Test2
{
  AsyncServiceFactory::AsyncServiceFactory(std::shared_ptr<IAsyncServiceProxyFactory> proxyFactory,
                                           std::shared_ptr<IAsyncServiceImplFactory> implFactory)
    : m_proxyFactory(std::move(proxyFactory))
    , m_implFactory(std::move(implFactory))
  {
    if (!m_proxyFactory)
    {
      throw new std::invalid_argument("AsyncServiceFactory: proxyFactory cannot be null");
    }

    if (!m_implFactory)
    {
      throw new std::invalid_argument("AsyncServiceFactory: implFactory cannot be null");
    }
  }

  AsyncServiceFactory::~AsyncServiceFactory() = default;


  std::span<const std::type_index> AsyncServiceFactory::GetSupportedInterfaces() const
  {
    return m_proxyFactory->GetSupportedInterfaces();
  }


  std::shared_ptr<IServiceProxyControl> AsyncServiceFactory::CreateProxy(const ServiceProxyCreateInfo& createInfo)
  {
    return m_proxyFactory->CreateProxy(createInfo);
  }


  std::shared_ptr<IServiceControl> AsyncServiceFactory::Create(const ServiceCreateInfo& createInfo)
  {
    return m_implFactory->Create(createInfo);
  }


  std::type_index AsyncServiceFactory::GetImplFactoryTypeId() const
  {
    return std::type_index(typeid(*m_implFactory));
  }
}
