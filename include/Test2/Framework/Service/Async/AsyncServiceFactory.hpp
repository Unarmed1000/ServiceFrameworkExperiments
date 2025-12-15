#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEFACTORY_HPP
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
#include <memory>

namespace Test2
{
  class IAsyncServiceProxyFactory;
  class IAsyncServiceImplFactory;

  class AsyncServiceFactory final : public IServiceFactoryInfo
  {
    std::shared_ptr<IAsyncServiceProxyFactory> m_proxyFactory;
    std::shared_ptr<IAsyncServiceImplFactory> m_implFactory;

  public:
    AsyncServiceFactory(std::shared_ptr<IAsyncServiceProxyFactory> proxyFactory, std::shared_ptr<IAsyncServiceImplFactory> implFactory);
    ~AsyncServiceFactory();

    std::span<const std::type_index> GetSupportedInterfaces() const final;

    [[nodiscard]]
    std::shared_ptr<IAsyncServiceProxyFactory> GetProxyFactory() const;

    [[nodiscard]]
    std::shared_ptr<IAsyncServiceImplFactory> GetImplFactory() const;
  };

}

#endif