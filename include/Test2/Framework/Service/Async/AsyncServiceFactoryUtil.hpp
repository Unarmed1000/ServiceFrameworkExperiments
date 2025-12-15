#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEFACTORYUTIL_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEFACTORYUTIL_HPP
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

namespace Test2::AsyncServiceFactoryUtil
{
  inline std::unique_ptr<AsyncServiceFactory> CreateAsyncServiceFactory(std::shared_ptr<IAsyncServiceProxyFactory> proxyFactory,
                                                                        std::shared_ptr<IAsyncServiceImplFactory> implFactory)
  {
    return std::make_unique<AsyncServiceFactory>(std::move(proxyFactory), std::move(implFactory));
  }

  template <typename TProxyFactory, typename TImplFactory>
  inline std::unique_ptr<AsyncServiceFactory> CreateAsyncServiceFactory()
  {
    return std::make_unique<AsyncServiceFactory>(std::make_shared<TProxyFactory>(), std::make_shared<TImplFactory>());
  }
}

#endif